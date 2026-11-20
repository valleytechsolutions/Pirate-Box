"""Validate and package the tracked public SD staging tree; never opens serial.

Rechecks every upstream archive member byte-for-byte and validates DDR1 through
the actual firmware ResourceIndex parser. Recreates the demo with the actual
BLE profile header and requires equality with the staged DDG1 file. Requires a
host C++17 compiler, not the ESP32 cross compiler. No new registry download.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT / '.work/piratebox-square-sd-bundle.zip')
    args = parser.parse_args()
    spec = json.loads((ROOT / 'sdcard-manifest.json').read_text(encoding='utf-8'))
    archive = ROOT / 'web/resources' / spec['upstream_archive']
    assert hashlib.sha256(archive.read_bytes()).hexdigest() == spec['upstream_archive_sha256'], 'Upstream archive changed'
    stage = ROOT / 'sdcard'
    with zipfile.ZipFile(archive) as source:
        for entry in source.infolist():
            if not entry.is_dir():
                assert (stage / entry.filename).resolve().is_relative_to(stage.resolve())
                assert (stage / entry.filename).read_bytes() == source.read(entry), f'Upstream member changed: {entry.filename}'
    from install_piratebox_square_sd import load_bundle
    entries = load_bundle(stage, ROOT / 'sdcard-manifest.json')
    expected = {entry['local'] for entry in entries}
    actual = {p.resolve() for p in stage.rglob('*') if p.is_file()}
    assert expected == actual, 'Staging tree contains unmanifested or missing files'
    compiler = args.compiler
    if compiler is None:
        located = shutil.which('g++')
        compiler = Path(located) if located else Path.home() / '.platformio/packages/toolchain-gccmingw32/bin/g++.exe'
    if not compiler.is_file():
        raise SystemExit('Provide --compiler with a host g++/clang++ supporting C++17')
    environment = dict(os.environ)
    environment['PATH'] = str(compiler.parent) + os.pathsep + environment.get('PATH', '')
    with tempfile.TemporaryDirectory(prefix='piratebox-sd-') as temp:
        out = Path(temp) / ('demo.exe' if os.name == 'nt' else 'demo')
        demo = Path(temp) / 'demo.ddg'
        subprocess.run([str(compiler), '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I'+str(ROOT/'firmware/include'),
                        str(ROOT/'tools/piratebox_sd_demo.cpp'), '-o', str(out)], check=True, env=environment)
        subprocess.run([str(out), str(stage/'dragon/resources/vendors.ddr'), str(demo)], check=True, env=environment)
        assert demo.read_bytes() == (stage/'dragon/profiles/piratebox-demo.ddg').read_bytes(), 'Staged BLE demo differs from actual firmware'
    args.output.parent.mkdir(parents=True, exist_ok=True)
    # Fixed metadata makes repeat packaging byte-identical across machines.
    with zipfile.ZipFile(args.output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as pack:
        for entry in entries:
            item = zipfile.ZipInfo(entry['path'].lstrip('/'), date_time=(2026, 10, 31, 0, 0, 0))
            item.compress_type = zipfile.ZIP_DEFLATED
            item.external_attr = 0o644 << 16
            pack.writestr(item, entry['local'].read_bytes())
        item = zipfile.ZipInfo('piratebox-bundle-manifest.json', date_time=(2026, 10, 31, 0, 0, 0))
        item.compress_type = zipfile.ZIP_DEFLATED; item.external_attr = 0o644 << 16
        pack.writestr(item, (ROOT/'sdcard-manifest.json').read_bytes())
    print(json.dumps({'archive': str(args.output), 'bytes': args.output.stat().st_size,
                      'sha256': hashlib.sha256(args.output.read_bytes()).hexdigest(),
                      'files': len(entries), 'usb_files': sum(bool(e['usb']) for e in entries)}, indent=2))


if __name__ == '__main__':
    main()
