"""Verify flash safeguards with temporary binary fixtures, without serial I/O."""
from pathlib import Path
import hashlib
import shutil
import struct
import tempfile

import piratebox_square_artifacts as artifacts


def rejected(action, text):
    try:
        action()
    except ValueError as error:
        assert text in str(error), str(error)
        return
    raise AssertionError('Unsafe artifact was accepted: ' + text)


with tempfile.TemporaryDirectory(prefix='piratebox-artifact-test-') as temp:
    root = Path(temp)
    boot = artifacts.ROOT / 'hardware/waveshare-c5-273/bootloader.bin'
    shutil.copyfile(boot, root / 'bootloader.bin')
    # The firmware fixture intentionally uses a second valid C5 image; these
    # tests check image integrity/target/geometry, not application semantics.
    shutil.copyfile(boot, root / 'firmware.bin')
    (root / 'boot_app0.bin').write_bytes(b'\xff' * 8192)
    table = b''.join(struct.pack('<HBBII16sI', 0x50AA, kind, subtype, offset, size, name.encode(), flags)
                     for name, kind, subtype, offset, size, flags in artifacts.EXPECTED)
    table += b'\xeb\xeb' + b'\xff' * 14 + hashlib.md5(table).digest()
    table = table.ljust(3072, b'\xff')
    (root / 'partitions.bin').write_bytes(table)
    result = artifacts.validate_images(root)
    assert [entry['offset'] for entry in result] == [0x2000, 0x8000, 0xe000, 0x10000]
    wrong_mode = bytearray(boot.read_bytes())
    wrong_mode[2] = 0  # QIO is invalid for this ROM boot stage, even with a valid digest.
    wrong_mode[-32:] = hashlib.sha256(wrong_mode[:-32]).digest()
    (root / 'bootloader.bin').write_bytes(wrong_mode)
    rejected(lambda: artifacts.validate_images(root), 'ROM bootloader header must use DIO')
    shutil.copyfile(boot, root / 'bootloader.bin')
    wrong_size = bytearray(boot.read_bytes())
    wrong_size[3] = (wrong_size[3] & 0x0f) | 0x40
    wrong_size[-32:] = hashlib.sha256(wrong_size[:-32]).digest()
    (root / 'bootloader.bin').write_bytes(wrong_size)
    rejected(lambda: artifacts.validate_images(root), 'Not configured for 32MB')
    shutil.copyfile(boot, root / 'bootloader.bin')
    corrupt = bytearray(table)
    corrupt[8] ^= 0x10
    (root / 'partitions.bin').write_bytes(corrupt)
    rejected(lambda: artifacts.validate_images(root), 'MD5 mismatch')
    (root / 'partitions.bin').write_bytes(table)
    damaged = bytearray(boot.read_bytes())
    damaged[-1] ^= 0x80
    (root / 'firmware.bin').write_bytes(damaged)
    rejected(lambda: artifacts.validate_images(root), 'SHA256 digest mismatch')
    shutil.copyfile(boot, root / 'firmware.bin')
    (root / 'bootloader.bin').write_bytes(b'\0' * 0x6001)
    rejected(lambda: artifacts.validate_images(root), 'oversized image')
    backup = root / 'backup.bin'
    backup.write_bytes(b'\0')
    rejected(lambda: artifacts.verify_backup(backup), 'exactly 32MiB')
    with backup.open('wb') as stream:
        stream.truncate(33554432)
    rejected(lambda: artifacts.verify_backup(backup), 'sidecar is missing')
    sidecar = Path(str(backup) + '.sha256')
    sidecar.write_text('0' * 64 + '  backup.bin\n')
    rejected(lambda: artifacts.verify_backup(backup), 'verification failed')
    actual = artifacts.digest(backup)
    sidecar.write_text(actual.upper() + '  backup.bin\n')
    assert artifacts.verify_backup(backup)['sha256'] == actual

print('C5 artifact/partition/backup safeguards PASS (no serial port opened)')
