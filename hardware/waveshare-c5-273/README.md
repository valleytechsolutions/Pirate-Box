# Pirate Box square C5 profile

Pirate Box 0.1.0 starts from Dragon Detector 0.13.42 and uses its full LVGL UI,
theme catalog, detector, history, clock/timer, recon and SD services. It is
for the Waveshare ESP32-C5-LCD-2.73, with a 320 x 320 ILI9488 display,
32 MB physical flash and 8 MB PSRAM.
[Hardware reference](https://docs.waveshare.com/ESP32-C5-LCD-2.73)

## Build

The `waveshare_c5_273` PlatformIO environment pins pioarduino 55.03.312
(Arduino 3.3.12 / ESP-IDF 5.5.5), NimBLE-Arduino 2.5.1 and the C5-capable
TFT_eSPI fork at `08eb2f73df08ad03cdbc0518cc2ad79118eb9ed0`.

Use the wrappers from the repository root. The build wrapper isolates the
PlatformIO core/packages and complete RISC-V compiler under
`%LOCALAPPDATA%\PirateBoxSquare\platformio`, with a private Python environment
in `%LOCALAPPDATA%\PirateBoxSquare\venv`, then sets
`PIRATEBOX_TOOLCHAIN_DIR` for `platformio.ini`. This prevents another firmware
project from replacing shared framework packages during compilation.

First provisioning requires verified compiler `14.2.0+20260121` and esptool
`5.4.0` seeds, normally from `.platformio\tools`. On another machine run
`tools/provision_piratebox_square.ps1` with `-SeedToolDirectory` and optional
`-SeedPython` pointing to matching local tools; the private copy is then reused.

```powershell
./tools/build_piratebox_square.ps1 -Clean -ArtifactDirectory '.\.work\piratebox-square-package'
./tools/flash_piratebox_square.ps1 -ArtifactDirectory '.\.work\piratebox-square-package' -Port COM79
```

The flash command above validates the receipt, images and preserved backup
without opening the serial port. Add `-Execute` to write the connected
authorized C5 board and independently byte-verify all four regions. `-Backup`
can select another preserved full 32 MiB backup. The wrapper writes the
bootloader at `0x2000`, partition table at `0x8000`, OTA selector at `0xe000`
and application at `0x10000`; the transfer excludes NVS, filesystem and SD data.
First boot initializes the upper `piratefs` volume only if its complete 16 MiB
range is verified erased. Nonblank volumes are preserved on mount failure.
The [board guide](../../docs/PIRATEBOX-SQUARE.md) documents the complete flow.

The checked-in bootloader belongs to this pinned framework. The pre-script
selects it because older esptool packages do not accept the newer builder's
hyphenated conversion flags. Regenerate the bootloader when changing cores.

## Board wiring and storage

| Function | Connection |
| --- | --- |
| KeyL / previous | GPIO2 |
| KeyR / next | GPIO3 |
| BOOT / enter | GPIO28 |
| Display SPI | SCLK6, MOSI7, MISO5, CS8, DC4 |
| microSD | Shared display SPI, CS9 |
| I2C | SDA27, SCL26 |
| LCD reset / backlight / battery ADC | CH32V003 at 0x24 |
| RTC | PCF85063 at 0x51 |
| Temperature and humidity | SHTC3 at 0x70 |
| Motion sensor | QMI8658 at 0x6B |
| Optional GPS receive | J5 pin 1 / GPIO12 |

The partition map retains the entire lower 16 MiB layout: two 5 MiB app slots,
the earlier 5.875 MiB filesystem, NVS and coredump. A new `piratefs` data volume
occupies the upper 16 MiB. C5 themes/badges/history and internal recon fallback
use this independent LittleFS volume; the previous lower filesystem is never
formatted or migrated automatically. Initial formatting is allowed only after
a full successful read proves the entire upper volume contains `0xFF` bytes.
The bootloader uses a DIO/80 MHz/32 MB header, then enables QIO internally.
Apps remain below 16 MiB; upper flash is raw data, not executable mapping.
SD logs and resources retain `/dragon` paths for compatibility. NimBLE host
buffers use PSRAM through the C5-only allocator override; worker stacks and
controller allocations remain internal.

## GPS and wardrive

The square board has no onboard GPS receiver. External UART GPS defaults to
disabled. GPS Setup can enable receive-only NMEA on GPIO12 at 4800, 9600,
19200, 38400, 57600 or 115200 baud. Connect receiver TX to GPIO12 and a
common ground; its signal must use 3.3 V logic. Power the receiver at its
rated voltage. Its RX input is not used by this firmware.

With an external receiver enabled, Wardrive uses the inherited fresh RMC/GGA
checks, WiFi/BLE counters, bounded log queue and finish-before-return flow.
The C5 scan driver enables both 2.4 and 5 GHz according to its country
policy, retains 5 GHz results and writes their frequency into WiGLE CSV.
It restores the previous radio band when the session ends. GPS can set an
unset RTC from a fresh fix. The RTC still requires power to retain time.

WiGLE and WDGoWars upload pages use the same `/wardriver.cfg` format and
receipt handling as Dragon Detector. Real uploads require the owner's
credentials and successful connectivity; build success is not validation
of either service.

## Hardware differences

This board has no touch controller or onboard speaker. It uses physical
navigation and visual alerts. The original S3 CSI interpretation and
SDK-specific raw-frame adapter are excluded from C5; no CSI or unrestricted
raw-transmission parity is claimed. GPS fixes, radio captures, sensor
accuracy and navigation require device validation in addition to compilation.
