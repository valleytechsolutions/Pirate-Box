"""Opt-in Pirate Box USB diagnostics with no reset, flash, upload or RF-tool commands.

Examples (the port is required; no serial device is auto-selected):
  py -3 tools/check_piratebox_square_device.py --port COM79 --commands status --commands "board health"
  py -3 tools/check_piratebox_square_device.py --port COM79 --screens menu weather radar-style clock
  py -3 tools/check_piratebox_square_device.py --port COM79 --theme-sweep
  py -3 tools/check_piratebox_square_device.py --port COM79 --sync-clock --timezone America/New_York
  py -3 tools/check_piratebox_square_device.py --self-test

Requires pyserial for device access. PNG decoding uses only the Python standard
library. Clock sync uses UTC plus a ZoneInfo-derived offset, never the host's
assumed local zone. Install tzdata or supply --utc-offset-minutes if needed.
The explicit theme sweep temporarily changes the saved theme and restores it.
Screenshots and theme checks change the displayed page; they do not start tools.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import struct
import sys
import time
import zlib

ROOT = Path(__file__).resolve().parents[1]
SCREENS = {
    "menu", "utilities", "weather", "radar", "radar-style", "clock", "stopwatch",
    "settings", "display", "health", "activity", "about", "targets", "wifi", "ble",
    "history", "notebook", "notebook-ble", "notebook-probes", "device-colors",
    "clock-settings", "clock-edit", "clock-sync", "date-settings", "time-settings",
    "orientation", "laps", "badge", "gps-setup", "wardrive", "wardrive-upload",
    "files", "captures", "modes", "tools", "startup",
}
DIAGNOSTICS = {
    "status", "memory status", "loop diagnostics", "scan status", "ui status",
    "ui layout", "radar status", "radar dots", "theme info", "clock status",
    "clock hardware", "clock diagnostics", "clock face", "orientation status",
    "stopwatch status", "stopwatch laps", "board health", "pbhealth", "pbkeys",
    "input status", "weather status", "imu status", "resources status", "registry status",
    "badge info", "badge checksum", "tool status", "lab status", "detector status",
    "axon status", "traffic status", "gps status", "gps external status",
    "gps reception", "wardrive status", "wardrive counters", "wardrive radio",
    "wardrive upload status", "radar themes", "radar feed status", "c5 display status",
}
RESPONSE_PREFIX = {
    "memory status": "MEMORY ", "loop diagnostics": "LOOP_END",
    "board health": "IMU_STATUS ", "pbhealth": "IMU_STATUS ",
    "resources status": "RESOURCES ", "ui status": "UI_STATUS ",
    "clock status": "CLOCK_STATUS ", "clock hardware": "CLOCK_HARDWARE ",
    "radar themes": "RADAR_THEMES_END", "radar feed status": "RADAR_FEED ",
}
FAILURE = re.compile(
    r"Guru Meditation|CORRUPT HEAP|assert(?:ion)? failed|Stack smashing|stack overflow|"
    r"panic(?:ked)?|abort\(\)|^FATAL\b|^ESP-ROM:|^rst:0x|^rst:|^Rebooting|"
    r"^(?:PIRATE BOX|DRAGON DETECTOR).* / boot$|^SYSTEM_RESTARTING",
    re.IGNORECASE,
)
COMMAND_ERROR = re.compile(r"^(?:[A-Z0-9_]*_ERROR\b|[A-Z0-9_]*_BUSY\b|Commands:)")


class ValidationError(RuntimeError):
    pass


def fields(line: str) -> dict[str, int]:
    return {key: int(value) for key, value in re.findall(r"(\w+)=(-?\d+)", line)}


def theme_catalog(path: Path) -> list[dict]:
    source = path.read_text(encoding="utf-8")
    arrays = []
    for name in ("radarThemeNames", "radarThemeCommands"):
        match = re.search(r"\b" + name + r"\[\]\s*=\s*\{(.*?)\};", source, re.S)
        if not match:
            raise ValidationError(f"Missing {name} in {path}")
        arrays.append([json.loads(value) for value in re.findall(r'"(?:[^"\\]|\\.)*"', match[1])])
    if not arrays[0] or len(arrays[0]) != len(arrays[1]) or len(set(arrays[1])) != len(arrays[1]):
        raise ValidationError("Theme catalog has mismatched or duplicate entries")
    return [{"index": i, "name": name, "command": arrays[1][i]} for i, name in enumerate(arrays[0])]


def rgb565_png(payload: bytes, width: int, height: int) -> bytes:
    """Decode the firmware's swapped RGB565 bytes as big-endian RGB, losslessly."""
    if not 1 <= width <= 1024 or not 1 <= height <= 1024 or len(payload) != width * height * 2:
        raise ValidationError("Invalid RGB565 geometry or byte count")
    scanlines = bytearray()
    for y in range(height):
        scanlines.append(0)  # PNG row filter: none
        for x in range(width):
            at = (y * width + x) * 2
            pixel = (payload[at] << 8) | payload[at + 1]
            r, g, b = (pixel >> 11) & 31, (pixel >> 5) & 63, pixel & 31
            scanlines.extend(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(scanlines), 6)) + chunk(b"IEND", b""))


class Monitor:
    def __init__(self):
        self.uptime = None
        self.reset_reason = None
        self.health_samples = 0
        self.failed = False

    def check(self, line: str):
        if FAILURE.search(line):
            self.failed = True
            raise ValidationError(f"Device reboot/crash indication: {line}")
        if line.startswith("HEALTH "):
            value = fields(line).get("uptime")
            if value is None:
                raise ValidationError("HEALTH has no uptime")
            # millis() wraps after 49.7 days; only accept a wrap near that boundary.
            if self.uptime is not None and value < self.uptime and not (self.uptime > 0xF0000000 and value < 0x0FFFFFFF):
                self.failed = True
                raise ValidationError(f"Uptime decreased: {self.uptime} -> {value}")
            self.uptime = value
            self.health_samples += 1
        if line.startswith("RUNTIME "):
            reason = fields(line).get("reset")
            if self.reset_reason is not None and reason != self.reset_reason:
                self.failed = True
                raise ValidationError(f"Reset reason changed: {self.reset_reason} -> {reason}")
            self.reset_reason = reason


class Device:
    def __init__(self, link, directory: Path, timeout: float = 8):
        self.link = link
        self.directory = directory
        self.timeout = timeout
        self.monitor = Monitor()
        self.buffer = bytearray()
        self.log = (directory / "serial.log").open("w", encoding="utf-8", newline="\n")
        self.report = {"commands": [], "screenshots": [], "themes": [], "failures": []}

    def close(self):
        self.log.close()

    def note(self, direction: str, text: str):
        stamp = datetime.now(timezone.utc).isoformat(timespec="milliseconds")
        self.log.write(f"{stamp} {direction} {text}\n")
        self.log.flush()

    def line(self, deadline: float) -> str | None:
        while time.monotonic() < deadline:
            end = self.buffer.find(b"\n")
            if end >= 0:
                raw = bytes(self.buffer[:end + 1])
                del self.buffer[:end + 1]
                line = raw.decode("utf-8", errors="replace").strip()
                if not line:
                    continue
                self.note("<", line)
                self.monitor.check(line)
                return line
            data = self.link.read(min(4096, max(1, self.link.in_waiting)))
            if data:
                self.buffer.extend(data)
                if len(self.buffer) > 16384:
                    raise ValidationError("Unframed serial text exceeds 16 KiB")
        return None

    def exact(self, count: int, timeout: float = 30) -> bytes:
        result = bytearray(self.buffer[:count])
        del self.buffer[:len(result)]
        deadline = time.monotonic() + timeout
        last = time.monotonic()
        while len(result) < count and time.monotonic() < deadline:
            part = self.link.read(min(4096, count - len(result)))
            if part:
                result.extend(part)
                last = time.monotonic()
            elif time.monotonic() - last > 6:
                break
        if len(result) != count:
            raise ValidationError(f"Screenshot transfer short: {len(result)} / {count} bytes")
        return bytes(result)

    def send(self, command: str):
        if "\n" in command or "\r" in command or len(command) > 180:
            raise ValidationError("Invalid serial command")
        self.note(">", command)
        wire = (command + "\n").encode("ascii")
        if self.link.write(wire) != len(wire):
            raise ValidationError("Short serial command write")
        self.link.flush()

    def drain(self, duration: float):
        deadline = time.monotonic() + duration
        while time.monotonic() < deadline:
            self.line(deadline)

    def command(self, command: str, prefix: str | None = None, settle: float = .4) -> list[str]:
        self.send(command)
        started = time.monotonic()
        deadline, last = started + self.timeout, started
        lines, matched = [], prefix is None
        while time.monotonic() < deadline:
            line = self.line(min(deadline, time.monotonic() + .1))
            if line:
                lines.append(line)
                last = time.monotonic()
                if COMMAND_ERROR.search(line):
                    raise ValidationError(f"{command}: {line}")
                if prefix is not None and line.startswith(prefix):
                    matched = True
            now = time.monotonic()
            if matched and now - started >= settle and now - last >= .15:
                break
        if not matched:
            raise ValidationError(f"No {prefix!r} response to {command!r}")
        self.report["commands"].append({"command": command, "lines": lines})
        return lines

    def query(self, command: str, prefix: str) -> str:
        return next(line for line in self.command(command, prefix) if line.startswith(prefix))

    def status(self):
        before = self.monitor.health_samples
        lines = self.command("status", "SESSION")
        if self.monitor.health_samples == before or not any(line.startswith("RUNTIME ") for line in lines):
            raise ValidationError("Incomplete HEALTH/RUNTIME status response")
        return fields(next(line for line in lines if line.startswith("HEALTH ")))

    def screenshot(self, name: str) -> dict:
        self.send("screenshot")
        deadline = time.monotonic() + self.timeout
        header = None
        while time.monotonic() < deadline:
            line = self.line(deadline)
            if line and line.startswith("SCREENSHOT_FAILED"):
                raise ValidationError(line)
            if line and (match := re.fullmatch(r"SCREENSHOT (\d+) (\d+) (\d+)", line)):
                header = tuple(map(int, match.groups()))
                break
        if header is None:
            raise ValidationError("No SCREENSHOT header")
        width, height, count = header
        if not 1 <= width <= 1024 or not 1 <= height <= 1024 or count != width * height * 2:
            raise ValidationError(f"Invalid SCREENSHOT header: {header}")
        payload = self.exact(count)
        trailer = self.line(time.monotonic() + self.timeout)
        match = re.fullmatch(r"SCREENSHOT_END ([0-9a-fA-F]{8})", trailer or "")
        if not match:
            raise ValidationError(f"Missing complete screenshot trailer: {trailer!r}")
        actual, expected = zlib.crc32(payload) & 0xFFFFFFFF, int(match[1], 16)
        if actual != expected:
            raise ValidationError(f"Screenshot CRC mismatch: {actual:08x} != {expected:08x}")
        filename = re.sub(r"[^a-zA-Z0-9_.-]", "-", name) + ".png"
        output = self.directory / filename
        output.write_bytes(rgb565_png(payload, width, height))
        item = {"path": str(output), "width": width, "height": height, "bytes": count, "crc32": f"{actual:08x}"}
        self.report["screenshots"].append(item)
        self.note("#", f"Screenshot verified {filename}: {count} bytes, CRC32 {actual:08x}")
        print(f"PASS screenshot {output} ({width}x{height}, CRC32 {actual:08x})", flush=True)
        return item


def validate_command(command: str):
    if command in DIAGNOSTICS:
        return
    if command.startswith("screen ") and command[7:] in SCREENS:
        return
    if command in {"c5 input next", "c5 input previous", "c5 input select", "c5 input back"}:
        return
    raise ValidationError(f"Command is outside the diagnostic/navigation allowlist: {command!r}")


def sync_clock(device: Device, zone_name: str, offset: int | None):
    if offset is None:
        from zoneinfo import ZoneInfo, ZoneInfoNotFoundError
        try:
            delta = datetime.now(timezone.utc).astimezone(ZoneInfo(zone_name)).utcoffset()
        except ZoneInfoNotFoundError as error:
            raise ValidationError(f"No timezone data for {zone_name}; install tzdata or pass --utc-offset-minutes") from error
        offset = int(delta.total_seconds() // 60)
    if not -720 <= offset <= 840:
        raise ValidationError("UTC offset must be between -720 and 840 minutes")
    device.query(f"clock offset {offset}", "CLOCK_OK")
    epoch = int(datetime.now(timezone.utc).timestamp())
    device.query(f"time {epoch}", "TIME_OK")
    result = fields(device.query("clock status", "CLOCK_STATUS "))
    hardware = fields(device.query("clock hardware", "CLOCK_HARDWARE "))
    now = int(datetime.now(timezone.utc).timestamp())
    if not result.get("valid") or result.get("offset") != offset or abs(result.get("epoch", 0) - now) > 5:
        raise ValidationError(f"Clock software readback failed: {result}")
    if not hardware.get("detected") or not hardware.get("valid") or abs(hardware.get("epoch", 0) - now) > 5:
        raise ValidationError(f"RTC hardware readback failed: {hardware}")
    device.report["clock_sync"] = {"timezone": zone_name, "utc_offset_minutes": offset, "software": result, "hardware": hardware}
    # A successful write/readback alone misses an oscillator that stops shortly
    # after the write. Keep the hardware validity check beyond its startup window.
    device.command("clock diagnostics", settle=2.5)
    delayed = fields(device.query("clock hardware", "CLOCK_HARDWARE "))
    now = int(datetime.now(timezone.utc).timestamp())
    device.report["clock_sync"]["delayed_hardware"] = delayed
    if not delayed.get("valid") or abs(delayed.get("epoch", 0) - now) > 5:
        raise ValidationError(f"RTC lost validity after write: {delayed}")
    print(f"PASS RTC UTC sync and delayed hardware readback; display offset {offset:+d} minutes", flush=True)


def sweep(device: Device, catalog: list[dict], pictures: bool, settle: float):
    original = fields(device.query("radar status", "RADAR_SETTINGS ")).get("theme")
    if original is None or not 0 <= original < len(catalog):
        raise ValidationError(f"Cannot restore unknown original theme {original}")
    personal = fields(device.query("theme info", "THEME_INFO ")).get("saved") == 1
    available = {int(line.split()[1]) for line in device.command("radar themes", prefix="RADAR_THEMES_END") if line.startswith("RADAR_THEME_AVAILABLE ")}
    if not available or original not in available:
        raise ValidationError(f"Original theme {original} is absent from the device available catalog")
    device.report["original_theme"] = catalog[original]
    try:
        for theme in catalog:
            if theme["index"] not in available:
                device.report["themes"].append({**theme, "skipped": "Unavailable on this board"})
                continue
            if theme["command"] == "custom" and not personal:
                device.report["themes"].append({**theme, "skipped": "No personal theme installed"})
                continue
            line = device.query("radar theme " + theme["command"], "RADAR_THEME ")
            if int(line.split()[1]) != theme["index"]:
                raise ValidationError(f"Theme selection mismatch: {line}")
            # Exercise the previously crashing Themes page for every palette.
            device.command("screen radar-style", settle=settle)
            memory = fields(device.query("memory status", "MEMORY "))
            health = device.status()
            selected = fields(device.query("radar status", "RADAR_SETTINGS "))
            if selected.get("theme") != theme["index"]:
                raise ValidationError(f"Theme did not persist during page render: {theme['name']}")
            # Exercise the clock after the previously crashing theme selector,
            # including the theme-to-face link and full page teardown/recreation.
            device.command("screen clock", settle=settle)
            face = fields(device.query("clock face", "CLOCK_FACE "))
            expected = {1: 0, 3: 1, 4: 8, 5: 2, 6: 9, 7: 10, 8: 11, 9: 12, 46: 0}.get(theme["index"], theme["index"] + 3)
            if face.get("index") != expected:
                raise ValidationError(f"Theme {theme['name']} selected clock {face}, expected {expected}")
            selected = fields(device.query("radar status", "RADAR_SETTINGS "))
            if selected.get("theme") != theme["index"]:
                raise ValidationError(f"Clock page changed selected theme {theme['name']}")
            device.status()
            item = {**theme, "memory": memory, "health": health, "clock_face": face, "passed": True}
            device.report["themes"].append(item)
            if pictures:
                device.screenshot(f"clock-{theme['index']:02d}-{theme['command']}")
            print(f"PASS theme {theme['index']:02d} {theme['name']}; clock={expected} heap={memory.get('heap')} psram={memory.get('psram')}", flush=True)
    finally:
        if device.monitor.failed:
            device.report["theme_restore"] = "Skipped after detected reboot/crash; no further writes sent"
        else:
            existing_failure = sys.exc_info()[0] is not None
            try:
                restored = device.query("radar theme " + catalog[original]["command"], "RADAR_THEME ")
                if int(restored.split()[1]) != original:
                    raise ValidationError(f"Original theme restoration mismatch: {restored}")
                device.status()
                device.report["theme_restore"] = catalog[original]
            except Exception as error:
                device.report["theme_restore"] = {"failed": str(error)}
                device.note("!", "Theme restoration failed: " + str(error))
                if not existing_failure:
                    raise
                device.report["failures"].append("Theme restoration also failed: " + str(error))


def self_test():
    import tempfile
    # RGB565 byte order and PNG content: red, green, blue, white.
    payload = bytes.fromhex("f80007e0001fffff")
    png = rgb565_png(payload, 2, 2)
    at, compressed = 8, bytearray()
    while at < len(png):
        count = struct.unpack(">I", png[at:at + 4])[0]
        kind, data = png[at + 4:at + 8], png[at + 8:at + 8 + count]
        assert struct.unpack(">I", png[at + 8 + count:at + 12 + count])[0] == zlib.crc32(kind + data) & 0xFFFFFFFF
        if kind == b"IDAT": compressed.extend(data)
        at += count + 12
    assert zlib.decompress(compressed) == b"\x00\xff\x00\x00\x00\xff\x00\x00\x00\x00\xff\xff\xff\xff"
    class Link:
        def __init__(self, wire): self.wire = bytearray(wire)
        @property
        def in_waiting(self): return len(self.wire)
        def read(self, count):
            count = min(count, 3)  # Fragment headers, binary and trailers.
            value = bytes(self.wire[:count]); del self.wire[:count]; return value
        def write(self, data): return len(data)
        def flush(self): pass
    crc = zlib.crc32(payload) & 0xFFFFFFFF
    wire = b"SCREENSHOT 2 2 8\r\n" + payload + f"\r\nSCREENSHOT_END {crc:08x}\r\n".encode()
    with tempfile.TemporaryDirectory() as temp:
        device = Device(Link(wire), Path(temp), timeout=.1)
        assert device.screenshot("test")["crc32"] == f"{crc:08x}"
        device.close()
        bad = Device(Link(wire.replace(f"{crc:08x}".encode(), b"00000000")), Path(temp), timeout=.1)
        try: bad.screenshot("bad")
        except ValidationError as error: assert "CRC mismatch" in str(error)
        else: raise AssertionError("Corrupt frame accepted")
        bad.close()
    monitor = Monitor(); monitor.check("HEALTH uptime=1000 heap=42"); monitor.check("RUNTIME reset=1 stack_free=300")
    monitor.check("HEALTH uptime=1001 heap=42"); monitor.check("RUNTIME reset=1 stack_free=300")
    try: monitor.check("HEALTH uptime=2 heap=42")
    except ValidationError: pass
    else: raise AssertionError("Reboot uptime accepted")
    for line in ("Guru Meditation Error", "FATAL: test", "PIRATE BOX 0.1.0 / board / boot", "ESP-ROM:esp32c5"):
        try: Monitor().check(line)
        except ValidationError: pass
        else: raise AssertionError(line)
    catalog = theme_catalog(ROOT / "firmware/include/radar_theme_catalog.h")
    assert catalog[2]["command"] == "gold" and catalog[34]["command"] == "custom" and len(catalog) == 47
    assert catalog[46]["command"] == "pirate"
    for command in ("system restart", "recon start", "put /dragon/test 3", "wardrive upload wigle"):
        try: validate_command(command)
        except ValidationError: pass
        else: raise AssertionError(command)
    print("PASS offline screenshot framing/CRC/PNG byte order, reset/uptime monitoring, catalog parsing and command allowlist")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", help="Explicit COM port; this tool never chooses a device")
    parser.add_argument("--commands", action="append", default=[], metavar="COMMAND", help="Repeat for each quoted diagnostic/navigation command")
    parser.add_argument("--screens", nargs="+", choices=sorted(SCREENS), default=[])
    parser.add_argument("--theme-sweep", action="store_true", help="Explicitly cycle all available catalog themes, then restore the original")
    parser.add_argument("--sweep-screenshots", action="store_true", help="Save a screenshot of every theme's Themes page; requires --theme-sweep")
    parser.add_argument("--sync-clock", action="store_true", help="Explicitly write current UTC to RTC and set the display offset")
    parser.add_argument("--timezone", default="America/New_York")
    parser.add_argument("--utc-offset-minutes", type=int, help="Explicit offset when IANA timezone data is unavailable")
    parser.add_argument("--output", type=Path, help="New/empty output directory; default .work/piratebox-validation/<UTC timestamp>")
    parser.add_argument("--timeout", type=float, default=8)
    parser.add_argument("--settle", type=float, default=.65)
    parser.add_argument("--self-test", action="store_true", help="Offline tests; never imports pyserial or opens a port")
    args = parser.parse_args()
    if args.self_test:
        self_test(); return 0
    if not args.port:
        parser.error("--port is required for device work")
    if args.sweep_screenshots and not args.theme_sweep:
        parser.error("--sweep-screenshots requires explicit --theme-sweep")
    if not .1 <= args.settle <= 10 or not 1 <= args.timeout <= 60:
        parser.error("--settle must be 0.1..10 seconds; --timeout must be 1..60 seconds")
    for command in args.commands:
        validate_command(command)
    directory = args.output or ROOT / ".work/piratebox-validation" / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    if directory.exists() and any(directory.iterdir()):
        raise ValidationError(f"Output directory is not empty: {directory}")
    directory.mkdir(parents=True, exist_ok=True)
    import serial
    link = serial.Serial(port=None, baudrate=115200, timeout=.1, write_timeout=5)
    link.dtr = False
    link.rts = False
    link.port = args.port
    device = Device(link, directory, args.timeout)
    device.report.update({"port": args.port, "started_utc": datetime.now(timezone.utc).isoformat(), "passed": False})
    try:
        link.open()  # DTR/RTS were set before opening; no reset or flush-input call.
        device.drain(.25)
        identity = device.query("badge info", "BADGE_INFO ")
        if "Pirate Box / Waveshare ESP32-C5-LCD-2.73" not in identity:
            raise ValidationError(f"Unexpected device identity: {identity}")
        device.report["identity"] = identity
        device.report["initial_health"] = device.status()
        if args.sync_clock:
            sync_clock(device, args.timezone, args.utc_offset_minutes)
        for command in args.commands or ([] if args.screens or args.theme_sweep or args.sync_clock else ["board health", "memory status", "input status", "clock status", "resources status"]):
            if command == "status":
                device.status()
            elif command.startswith("screen "):
                device.command(command, settle=args.settle)
                device.query("ui status", "UI_STATUS ")
            else:
                lines = device.command(command, RESPONSE_PREFIX.get(command))
                if not lines:
                    raise ValidationError(f"No response to {command!r}")
            print(f"PASS command {command}", flush=True)
        if args.theme_sweep:
            sweep(device, theme_catalog(ROOT / "firmware/include/radar_theme_catalog.h"), args.sweep_screenshots, args.settle)
        for index, page in enumerate(args.screens):
            device.command("screen " + page, settle=args.settle)
            device.query("ui status", "UI_STATUS ")
            device.screenshot(f"{index + 1:02d}-{page}")
            device.status()
        device.report["final_health"] = device.status()
        device.report["passed"] = True
    except Exception as error:
        device.report["failures"].append(f"{type(error).__name__}: {error}")
        device.note("!", str(error))
        print(f"FAIL {error}", file=sys.stderr, flush=True)
    finally:
        device.report["finished_utc"] = datetime.now(timezone.utc).isoformat()
        device.report["health_samples"] = device.monitor.health_samples
        (directory / "report.json").write_text(json.dumps(device.report, indent=2) + "\n", encoding="utf-8")
        device.close()
        if link.is_open:
            link.close()
        print(f"Evidence: {directory}", flush=True)
    return 0 if device.report["passed"] else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValidationError, ImportError) as error:
        print(f"FAIL {error}", file=sys.stderr)
        raise SystemExit(1)
