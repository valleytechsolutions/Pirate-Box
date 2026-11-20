"""Record/check reproducible C5 build provenance and exact flash geometry.

This helper never opens a serial port. The PowerShell wrappers perform builds
and explicit flashing only after these checks succeed.
"""
from __future__ import annotations

import argparse
import csv
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
ENVIRONMENT = 'waveshare_c5_273'
PARTITION = ROOT / 'hardware/waveshare-c5-273/partitions.csv'
SEGMENTS = {
    'bootloader.bin': (0x2000, 0x6000),
    'partitions.bin': (0x8000, 0x1000),
    'boot_app0.bin': (0xE000, 0x2000),
    'firmware.bin': (0x10000, 0x500000),
}
EXPECTED = [
    ('nvs', 1, 2, 0x9000, 0x5000, 0),
    ('otadata', 1, 0, 0xE000, 0x2000, 0),
    ('app0', 0, 0x10, 0x10000, 0x500000, 0),
    ('app1', 0, 0x11, 0x510000, 0x500000, 0),
    ('spiffs', 1, 0x82, 0xA10000, 0x5E0000, 0),
    ('coredump', 1, 3, 0xFF0000, 0x10000, 0),
    ('piratefs', 1, 0x82, 0x1000000, 0x1000000, 0),
]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def file_record(path, label=None):
    path = Path(path)
    return {'path': label or str(path.resolve()), 'bytes': path.stat().st_size, 'sha256': digest(path)}


def source_snapshot():
    # Include generated embedded artwork, local libraries, board scripts and
    # build tools. Exclude logs, caches and outputs from their own receipt.
    files = {ROOT / 'platformio.ini', ROOT / 'dependencies.lock'}
    extensions = {'.h', '.hpp', '.c', '.cc', '.cpp', '.inc', '.ino', '.s', '.S', '.ld',
                  '.py', '.ps1', '.json', '.ini', '.csv', '.bin', '.txt', '.cmake'}
    for directory in ('firmware', 'hardware', 'lib', 'tools'):
        for path in (ROOT / directory).rglob('*'):
            if path.is_file() and path.suffix in extensions and not any(
                    part in {'.git', '.pio', '.work', '__pycache__', 'node_modules'} for part in path.parts):
                files.add(path)
    return [file_record(path, path.relative_to(ROOT).as_posix()) for path in sorted(files)]


def write_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def assert_snapshot(expected):
    actual = source_snapshot()
    if actual != expected:
        old = {entry['path']: entry['sha256'] for entry in expected}
        new = {entry['path']: entry['sha256'] for entry in actual}
        changed = sorted(path for path in old.keys() | new.keys() if old.get(path) != new.get(path))
        raise ValueError('Source changed during/after build; rebuild required: ' + ', '.join(changed[:25]))


def check_partition(artifact):
    type_codes = {'app': 0, 'data': 1}
    subtypes = {'nvs': 2, 'ota': 0, 'ota_0': 0x10, 'ota_1': 0x11, 'spiffs': 0x82, 'coredump': 3}
    rows = []
    for fields in csv.reader(PARTITION.read_text().splitlines()):
        if not fields or fields[0].strip().startswith('#'):
            continue
        fields = [field.strip() for field in fields]
        require(len(fields) >= 5, 'Malformed partition CSV')
        require(len(fields) < 6 or not fields[5], 'Unexpected partition flags')
        rows.append((fields[0], type_codes[fields[1]], subtypes[fields[2]], int(fields[3], 0), int(fields[4], 0), 0))
    require(rows == EXPECTED, 'Partition source differs from reviewed C5 32MiB layout with lower app slots')
    raw = (artifact / 'partitions.bin').read_bytes()
    decoded = []
    md5_seen = False
    for offset in range(0, len(raw), 32):
        entry = raw[offset:offset + 32]
        require(len(entry) == 32, 'Partition table has a partial entry')
        if entry == b'\xff' * 32:
            require(all(byte == 0xFF for byte in raw[offset:]), 'Unexpected trailing partition bytes')
            break
        if entry[:2] == b'\xeb\xeb':
            require(not md5_seen and entry[2:16] == b'\xff' * 14, 'Malformed partition MD5 record')
            require(entry[16:] == hashlib.md5(raw[:offset]).digest(), 'Partition binary MD5 mismatch')
            md5_seen = True
            continue
        require(not md5_seen, 'Partition entry follows MD5 marker')
        magic, kind, subtype, address, size, name, flags = struct.unpack('<HBBII16sI', entry)
        require(magic == 0x50AA, 'Invalid partition binary magic')
        decoded.append((name.rstrip(b'\0').decode('ascii'), kind, subtype, address, size, flags))
    require(md5_seen and decoded == EXPECTED, 'Partition binary does not match reviewed CSV')


def validate_images(artifact):
    load_esptool()
    from esptool.bin_image import ESP32C5FirmwareImage
    import esptool
    require(esptool.__version__.startswith('5.4.'), 'Use the reviewed esptool 5.4.x runtime')
    entries = []
    for name, (address, maximum) in SEGMENTS.items():
        path = artifact / name
        require(path.is_file() and 0 < path.stat().st_size <= maximum, f'Missing/oversized image: {name}')
        require(address + path.stat().st_size <= 0x1000000, f'Image crosses 16MiB boundary: {name}')
        entries.append({**file_record(path, name), 'offset': address})
    for name in ('bootloader.bin', 'firmware.bin'):
        with (artifact / name).open('rb') as stream:
            image = ESP32C5FirmwareImage(stream)
        image.verify()
        require(image.chip_id == 23, f'Not an ESP32-C5 image: {name}')
        if name == 'bootloader.bin':
            require(image.flash_mode == 2, 'C5 ROM bootloader header must use DIO; QIO is enabled by the second stage')
        require(image.flash_size_freq >> 4 == 5, f'Not configured for 32MB flash: {name}')
        require(image.calculate_checksum() == image.checksum, f'Image checksum mismatch: {name}')
        require(image.append_digest and image.stored_digest == image.calc_digest, f'Image SHA256 digest mismatch: {name}')
    check_partition(artifact)
    return entries


def load_esptool():
    """Import the explicit private tool, independent of global pip changes."""
    core = Path(os.environ.get('PLATFORMIO_CORE_DIR', Path(os.environ.get('LOCALAPPDATA', Path.home())) / 'PirateBoxSquare/platformio'))
    selected = os.environ.get('PIRATEBOX_ESPTOOL_ROOT')
    candidates = [Path(selected)] if selected else [core / 'tools/tool-esptoolpy', core / 'packages/tool-esptoolpy']
    for candidate in candidates:
        if (candidate / 'esptool/__init__.py').is_file():
            sys.path.insert(0, str(candidate.resolve()))
            import esptool
            require(Path(esptool.__file__).resolve().is_relative_to(candidate.resolve()), 'Unexpected esptool import origin')
            require(esptool.__version__.startswith('5.4.'), 'Use the reviewed esptool 5.4.x runtime')
            return candidate.resolve()
    raise ValueError('Private esptool 5.4 tool missing; run provision_piratebox_square.ps1')


def external_dependencies():
    core = Path(os.environ.get('PLATFORMIO_CORE_DIR', Path(os.environ.get('LOCALAPPDATA', Path.home())) / 'PirateBoxSquare/platformio'))
    packages = Path(os.environ.get('PLATFORMIO_PACKAGES_DIR', core / 'packages'))
    candidates = [packages / directory / 'package.json' for directory in
                  ('framework-arduinoespressif32', 'framework-arduinoespressif32-libs', 'toolchain-riscv32-esp')]
    candidates += [core / 'tools/toolchain-riscv32-esp/bin/riscv32-esp-elf-g++.exe']
    esptool_root = load_esptool()
    candidates += [esptool_root / 'package.json']
    candidates += sorted((esptool_root / 'esptool').rglob('*.py'))
    libdeps = Path(os.environ.get('PLATFORMIO_LIBDEPS_DIR', ROOT / '.pio/libdeps')) / ENVIRONMENT
    if libdeps.is_dir():
        candidates += sorted(path for path in libdeps.rglob('*') if path.is_file() and
                             path.suffix in {'.h', '.hpp', '.c', '.cpp', '.inc', '.S', '.json'} and
                             '.git' not in path.parts)
    return [file_record(path) for path in candidates if path.is_file()]


def verify_backup(path):
    require(path.is_file() and path.stat().st_size == 33554432, 'Backup must contain exactly 32MiB')
    sidecar = Path(str(path) + '.sha256')
    require(sidecar.is_file(), 'Backup .sha256 sidecar is missing')
    match = re.match(r'^\s*([0-9a-fA-F]{64})(?:\s|$)', sidecar.read_text(encoding='utf-8-sig'))
    require(match is not None, 'Malformed backup SHA256 sidecar')
    actual = digest(path)
    require(actual == match.group(1).lower(), 'Backup SHA256 verification failed')
    return file_record(path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('action', choices=('snapshot', 'record', 'verify'))
    parser.add_argument('--snapshot', type=Path)
    parser.add_argument('--artifact', type=Path)
    parser.add_argument('--backup', type=Path)
    args = parser.parse_args()
    if args.action == 'snapshot':
        require(args.snapshot is not None, 'Snapshot path required')
        write_json(args.snapshot, {'sources': source_snapshot(), 'dependencies': external_dependencies()})
        print('Source snapshot saved:', args.snapshot)
        return
    require(args.artifact is not None, 'Artifact directory required')
    artifact = args.artifact.resolve()
    receipt_path = artifact / 'build-receipt.json'
    if args.action == 'record':
        require(args.snapshot is not None and args.snapshot.is_file(), 'A pre-build snapshot is required')
        before = json.loads(args.snapshot.read_text())
        assert_snapshot(before['sources'])
        require(before['dependencies'] == external_dependencies(), 'Toolchain changed during build; rebuild required')
        binaries = validate_images(artifact)
        commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
        status = subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True).splitlines()
        receipt = {'schema': 1, 'environment': ENVIRONMENT, 'chip': 'esp32c5', 'flashSizeBytes': 33554432,
                   'flashAddressLimit': 0x2000000, 'appAddressLimit': 0x1000000, 'builtAtUtc': datetime.now(timezone.utc).isoformat(),
                   'sourceCommit': commit, 'sourceChanges': status, 'sourceHashes': before['sources'],
                   'dependencies': before['dependencies'], 'partitionCsvSha256': digest(PARTITION), 'binaries': binaries}
        write_json(receipt_path, receipt)
        (artifact / 'SHA256SUMS.txt').write_text(''.join(f"{entry['sha256']}  {entry['path']}\n" for entry in binaries))
        print('Verified C5 images and stable source/toolchain receipt:', receipt_path)
        return
    require(receipt_path.is_file(), 'Build receipt is missing; use build_piratebox_square.ps1')
    receipt = json.loads(receipt_path.read_text(encoding='utf-8-sig'))
    require(receipt.get('schema') == 1 and receipt.get('environment') == ENVIRONMENT and
            receipt.get('chip') == 'esp32c5' and receipt.get('flashSizeBytes') == 33554432 and
            receipt.get('flashAddressLimit') == 0x2000000 and receipt.get('appAddressLimit') == 0x1000000,
            'Receipt target is not reviewed C5/32MiB configuration with lower app slots')
    require(receipt.get('partitionCsvSha256') == digest(PARTITION), 'Partition CSV changed since build')
    require(receipt.get('sourceHashes'), 'Receipt lacks source hashes')
    assert_snapshot(receipt['sourceHashes'])
    require(receipt.get('dependencies') == external_dependencies(), 'Toolchain changed since build')
    require(receipt.get('binaries') == validate_images(artifact), 'Artifact SHA256 or image metadata differs from receipt')
    require(args.backup is not None, 'Verified 32MiB backup path required')
    backup = verify_backup(args.backup.resolve())
    print('Artifact, source, partition, image headers and full backup verified.')
    print('Backup SHA256:', backup['sha256'])


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, KeyError, json.JSONDecodeError) as exc:
        print('PIRATEBOX_ARTIFACT_ERROR:', exc, file=sys.stderr)
        sys.exit(1)
