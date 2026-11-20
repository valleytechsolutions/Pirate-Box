"""Plan, verify, or add missing public SD resources over Pirate Box USB.

No port means a local manifest check only. --port verifies without writing;
--execute permits new files. Existing files (including .part files) are never
replaced or deleted. The board's PUT protocol commits completed files by rename.
Every new file is read back through GET and checked with SHA-256. Reruns resume
at file boundaries, not within an interrupted file. No reset or RF commands.

  python tools/install_piratebox_square_sd.py
  python tools/install_piratebox_square_sd.py --port COM79
  python tools/install_piratebox_square_sd.py --port COM79 --execute
  python tools/install_piratebox_square_sd.py --self-test

Device access requires pyserial. The private build environment includes it.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
MAX_BYTES = 16 * 1024 * 1024
ERROR = re.compile(r"^(?:[A-Z0-9_]*_ERROR\b|[A-Z0-9_]*_BUSY\b|Commands:)")
CRASH = re.compile(r"Guru Meditation|CORRUPT HEAP|assert(?:ion)? failed|Stack smashing|"
                   r"stack overflow|^ESP-ROM:|^rst:|^Rebooting|^SYSTEM_RESTARTING| / boot$", re.I)


class TransferError(RuntimeError):
    pass


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def load_bundle(bundle: Path, manifest: Path) -> list[dict]:
    spec = json.loads(manifest.read_text(encoding="utf-8-sig"))
    if spec.get("schema") != 1 or not isinstance(spec.get("files"), list):
        raise TransferError("Unsupported bundle manifest")
    result, seen = [], set()
    for entry in spec["files"]:
        path = entry.get("path", "")
        if (not re.fullmatch(r"/[A-Za-z0-9_/.-]+", path) or ".." in path
                or "//" in path or path in seen):
            raise TransferError(f"Unsafe or repeated bundle path: {path!r}")
        seen.add(path)
        local = (bundle / path.lstrip("/")).resolve()
        if not local.is_relative_to(bundle.resolve()) or not local.is_file():
            raise TransferError(f"Missing or out-of-bundle file: {path}")
        data = local.read_bytes()
        if not 0 < len(data) <= MAX_BYTES or len(data) != entry.get("bytes") or digest(data) != entry.get("sha256"):
            raise TransferError(f"Local size/hash mismatch: {path}")
        if entry.get("usb"):
            if not path.startswith(("/dragon/resources/", "/dragon/profiles/")) or ".cfg" in path.lower():
                raise TransferError(f"Path is outside public USB bundle directories: {path}")
        result.append(dict(entry, local=local))
    if not result:
        raise TransferError("Empty bundle manifest")
    return result


class Device:
    def __init__(self, link, *, timeout=8.0, pace=.002, progress=print):
        self.link, self.timeout, self.pace = link, timeout, pace
        self.progress = progress
        self.buffer = bytearray()

    def send(self, command):
        if not re.fullmatch(r"[ -~]{1,180}", command):
            raise TransferError("Unsafe command text")
        self.write((command + "\n").encode("ascii"))

    def write(self, data):
        if self.link.write(data) != len(data):
            raise TransferError("Short serial write; stop and rerun after the receiver times out")
        self.link.flush()

    def line(self, deadline):
        while time.monotonic() < deadline:
            end = self.buffer.find(b"\n")
            if end >= 0:
                raw = bytes(self.buffer[:end + 1])
                del self.buffer[:end + 1]
                line = raw.decode("utf-8", errors="replace").strip()
                if not line:
                    continue
                if CRASH.search(line):
                    raise TransferError(f"Device reboot/crash during transfer: {line}")
                return line
            data = self.link.read(min(4096, max(1, self.link.in_waiting)))
            if data:
                self.buffer.extend(data)
                if len(self.buffer) > 32768:
                    raise TransferError("Unframed USB text exceeds 32 KiB")
        raise TransferError("Timed out waiting for a USB response")

    def response(self, prefix, *, timeout=None):
        deadline = time.monotonic() + (self.timeout if timeout is None else timeout)
        while True:
            line = self.line(deadline)
            if line.startswith(prefix):
                return line
            if ERROR.search(line):
                raise TransferError(line)

    def query(self, command, prefix):
        self.send(command)
        return self.response(prefix)

    def check_board(self):
        badge = self.query("badge info", "BADGE_INFO ")
        if not re.fullmatch(r"BADGE_INFO \S+ 320 (?:READY|NO_STORAGE) Pirate Box / Waveshare ESP32-C5-LCD-2\.73", badge):
            raise TransferError(f"Unexpected board identity: {badge}")
        health = self.query("board health", "BOARD_HEALTH ")
        if not re.search(r"\bsd=1\b", health):
            raise TransferError(f"SD card is not mounted: {health}")
        scan = self.query("scan status", "SCAN_STATUS ")
        if not re.search(r"\blab=0\b", scan):
            raise TransferError("Stop the active radio tool before installing SD resources")
        lab = self.query("lab status", "LAB_STATUS ")
        if not re.search(r"\bowned=0\b", lab):
            raise TransferError("Stop the active BLE Lab operation before installing SD resources")
        return {"identity": badge, "health": health, "scan": scan, "lab": lab}

    def get_hash(self, path):
        self.send("get " + path)
        deadline = time.monotonic() + self.timeout
        while True:
            line = self.line(deadline)
            if line == "GET_ERROR":
                return None
            match = re.fullmatch(r"FILE_BYTES (\d+)", line)
            if match:
                count = int(match[1])
                break
            if ERROR.search(line):
                raise TransferError(line)
        if count > MAX_BYTES:
            raise TransferError(f"Refusing oversized remote file ({count} bytes): {path}")
        checksum = hashlib.sha256()
        remaining = count
        deadline = time.monotonic() + max(30, count / 4000)
        idle = time.monotonic()
        while remaining:
            if self.buffer:
                data = bytes(self.buffer[:remaining])
                del self.buffer[:len(data)]
            else:
                data = self.link.read(min(4096, remaining))
            if data:
                checksum.update(data)
                remaining -= len(data)
                idle = time.monotonic()
            elif time.monotonic() - idle > 8 or time.monotonic() > deadline:
                raise TransferError(f"Short GET for {path}: {count - remaining}/{count} bytes")
        self.response("FILE_END")
        return {"bytes": count, "sha256": checksum.hexdigest()}

    def put(self, path, data):
        self.send(f"put {path} {len(data)}")
        self.response("PUT_READY")
        sent, acknowledged, announced = 0, 0, 0
        while sent < len(data):
            end = min(sent + 512, len(data))
            while sent < end:
                part = data[sent:min(sent + 64, end)]
                self.write(part)
                sent += len(part)
                if self.pace:
                    time.sleep(self.pace)
            deadline = time.monotonic() + min(self.timeout, 4)
            while acknowledged < sent:
                line = self.line(deadline)
                if line == "PUT_OK":
                    if sent != len(data):
                        raise TransferError("Premature PUT_OK")
                    return
                match = re.fullmatch(r"PUT_ACK (\d+)", line)
                if match:
                    offset = int(match[1])
                    if offset > sent:
                        raise TransferError(f"Impossible PUT_ACK offset {offset}/{sent}")
                    acknowledged = max(acknowledged, offset)
                elif ERROR.search(line):
                    raise TransferError(line)
            if acknowledged - announced >= 256 * 1024:
                self.progress(f"  {path}: {acknowledged}/{len(data)} bytes received", flush=True)
                announced = acknowledged
        self.response("PUT_OK")


def reconcile(device, files, execute, result=None):
    if result is None:
        result = []
    for entry in files:
        row = {k: entry[k] for k in ("path", "bytes", "sha256", "usb")}
        result.append(row)
        if not entry["usb"]:
            row["status"] = "manual-only-documentation"
            continue
        path = entry["path"]
        remote = device.get_hash(path)
        if remote is not None:
            row["remote"] = remote
            row["status"] = "verified-existing" if remote == {"bytes": entry["bytes"], "sha256": entry["sha256"]} else "conflict-preserved"
        elif device.get_hash(path + ".part") is not None:
            row["status"] = "partial-preserved"
        elif not execute:
            row["status"] = "missing"
        else:
            row["status"] = "install-in-progress"
            device.put(path, entry["local"].read_bytes())
            remote = device.get_hash(path)
            row["remote"] = remote
            if remote != {"bytes": entry["bytes"], "sha256": entry["sha256"]}:
                row["status"] = "readback-mismatch-preserved"
                raise TransferError(f"SHA-256 readback mismatch: {path}; file preserved for inspection")
            row["status"] = "installed-verified"
        print(f"{row['status']}: {path}", flush=True)
    return result


def self_test():
    import tempfile
    class FakeSerial:
        def __init__(self, files=None, *, corrupt=False):
            self.files = dict(files or {})
            self.incoming = bytearray()
            self.command = bytearray()
            self.upload = None
            self.put_count = 0
            self.corrupt = corrupt
        @property
        def in_waiting(self):
            return len(self.incoming)
        def flush(self):
            pass
        def emit(self, value):
            self.incoming.extend(value)
        def read(self, count):
            # Fragment all text and binary, including headers, at awkward points.
            count = min(count, 7, len(self.incoming))
            part = bytes(self.incoming[:count]);del self.incoming[:count]
            return part
        def write(self, data):
            if self.upload is not None:
                path, total, payload = self.upload
                payload.extend(data)
                # Duplicate/lagging ACKs occur in the actual native USB protocol.
                self.emit(f"PUT_ACK 0\nPUT_ACK {len(payload)}\n".encode())
                if len(payload) == total:
                    self.files[path] = bytes(payload) + (b"x" if self.corrupt else b"")
                    self.upload = None
                    self.emit(b"PUT_OK\n")
                assert len(payload) <= total
            else:
                self.command.extend(data)
                if b"\n" in self.command:
                    raw, _, tail = self.command.partition(b"\n")
                    assert not tail;self.command.clear()
                    command = raw.decode()
                    if command.startswith("get "):
                        path = command[4:]
                        if path not in self.files:self.emit(b"GET_ERROR\n")
                        else:
                            payload = self.files[path]
                            self.emit(f"FILE_BYTES {len(payload)}\n".encode() + payload + b"\nFILE_END\n")
                    elif command.startswith("put "):
                        _, path, size = command.split()
                        assert path not in self.files and path + '.part' not in self.files
                        self.put_count += 1
                        self.upload = (path, int(size), bytearray())
                        self.emit(b"PUT_READY\n")
                    else:raise AssertionError(command)
            return len(data)
    with tempfile.TemporaryDirectory() as folder:
        payload = bytes(range(256)) * 13 + b"\nFILE_END\nPUT_OK\n\x00"
        path = Path(folder) / "test.ddr";path.write_bytes(payload)
        entry = {"path": "/dragon/resources/test.ddr", "bytes": len(payload), "sha256": digest(payload), "usb": True, "local": path}
        serial = FakeSerial()
        rows = reconcile(Device(serial, timeout=.2, pace=0), [entry], False)
        assert rows[0]["status"] == "missing" and serial.put_count == 0
        rows = reconcile(Device(serial, timeout=.2, pace=0), [entry], True)
        assert rows[0]["status"] == "installed-verified" and serial.files[entry["path"]] == payload
        rows = reconcile(Device(serial, timeout=.2, pace=0), [entry], True)
        assert rows[0]["status"] == "verified-existing" and serial.put_count == 1
        for suffix, content, expected in [("", b"user file", "conflict-preserved"), (".part", b"interrupted", "partial-preserved")]:
            serial = FakeSerial({entry["path"] + suffix: content})
            before = dict(serial.files)
            rows = reconcile(Device(serial, timeout=.2, pace=0), [entry], True)
            assert rows[0]["status"] == expected and serial.put_count == 0 and serial.files == before
        serial = FakeSerial(corrupt=True)
        try:reconcile(Device(serial, timeout=.2, pace=0), [entry], True)
        except TransferError as error:assert "readback mismatch" in str(error)
        else:raise AssertionError("Corrupt readback accepted")
        serial = FakeSerial();serial.emit(b"FILE_BYTES 4\n\x00\n")
        device = Device(serial, timeout=.001, pace=0)
        try:device.response("PUT_READY")
        except TransferError:pass
        else:raise AssertionError("Malformed response accepted")
    print("PASS: fragmented binary/text, ACK duplication, read-only mode, file-level resume, preserved conflicts/partials, corrupt readback and malformed response")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--bundle", type=Path, default=ROOT / "sdcard")
    parser.add_argument("--manifest", type=Path, default=ROOT / "sdcard-manifest.json")
    parser.add_argument("--port", help="Explicit serial port; omitted means local checks only")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--execute", action="store_true", help="Add missing files; never overwrite existing files")
    parser.add_argument("--report", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test();return 0
    if args.execute and not args.port:
        parser.error("--execute requires an explicit --port")
    files = load_bundle(args.bundle, args.manifest)
    report = {"started": datetime.now(timezone.utc).isoformat(), "mode": "install" if args.execute else "verify" if args.port else "local-plan", "files": []}
    if not args.port:
        for entry in files:
            print(f"{'USB' if entry['usb'] else 'MANUAL'} {entry['bytes']:>9} {entry['path']} {entry['sha256']}")
        print(f"Local SHA-256 checks passed: {len(files)} files; no device opened")
        return 0
    destination = args.report or ROOT / ".work" / ("sd-resources-" + datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S") + ".json")
    destination.parent.mkdir(parents=True, exist_ok=True)
    exit_code = 1
    try:
        import serial
        # Configure before opening to avoid toggling reset/boot control lines.
        link = serial.Serial(port=None, baudrate=args.baud, timeout=.1, write_timeout=4)
        link.dtr = False;link.rts = False;link.port = args.port
        with link:
            device = Device(link)
            report["device"] = device.check_board()
            report["resources_before"] = device.query("resources status", "RESOURCES ")
            reconcile(device, files, args.execute, report["files"])
            report["resources_after"] = device.query("resources status", "RESOURCES ")
        ok = all(row["status"] in ("verified-existing", "installed-verified", "manual-only-documentation") for row in report["files"])
        report["verified_all_usb_files"] = ok
        report["restart_needed_for_new_vendor_pack"] = any(row["path"].endswith("/vendors.ddr") and row["status"] == "installed-verified" for row in report["files"])
        if report["restart_needed_for_new_vendor_pack"]:
            print("Vendor pack verified on SD. Restart firmware, then check resources status to confirm it loaded into PSRAM.")
        exit_code = 0 if ok else 2
    except Exception as error:
        report["error"] = f"{type(error).__name__}: {error}"
        print(report["error"], file=sys.stderr)
    finally:
        report["finished"] = datetime.now(timezone.utc).isoformat()
        destination.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(f"Report: {destination}", flush=True)
    return exit_code


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except TransferError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(1)
