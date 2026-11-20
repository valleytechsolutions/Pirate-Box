# Pirate Box — Waveshare ESP32-C5-LCD-2.73

This adapter uses the board's 320 × 320 ILI9488 LCD, microSD, PCF85063 RTC,
SHTC3 temperature/humidity sensor, QMI8658 accelerometer/gyroscope and CH32
backlight/battery ADC. The physical module provides 32 MB flash and 8 MB PSRAM.
The LCD has no touch controller. Optional GPS uses the UART header; GPS is not
built into this board.

## Controls

| Control | Connection | Action |
|---|---|---|
| KeyL | ESP32 GPIO2 | Previous / up |
| KeyR | ESP32 GPIO3 | Next / down |
| BOOT short press | ESP32 GPIO28 | Select on release |
| BOOT hold | ESP32 GPIO28 | Back after 800 ms; release does not also select |

The controls drive Dragon Detector's dynamic LVGL focus group, including
dialogs and scrolling lists. Holding a navigation key repeats after 450 ms.
A button held during startup must be released before it can act.

## Optional rotary encoder, five-way joystick, or five buttons

Use the CH32 **EXIO4 through EXIO14** on the expansion header. These labels
are CH32 expander channels, **not ESP32 GPIO numbers**. Firmware refuses
reserved channels and duplicate pin assignments. EXIO4–14 remain inputs even
when expansion controls are disabled. Existing onboard buttons always work.

Use a digital five-switch joystick, five normally-open buttons, or a mechanical
quadrature encoder with a push switch. Connect each contact between its EXIO
channel and GND, and provide an external 10 kΩ pull-up from each input to 3.3 V.
Do not use a 5 V encoder module or an analog joystick through this interface.
Verify the physical header labels against the board schematic before wiring.

Examples in the USB serial console:

```text
input encoder 4 5 6
input joystick 4 5 6 7 8
input buttons 4 5 6 7 8
input off
input status
```

Encoder arguments are A, B and select; swap A and B to reverse rotation.
Joystick/button arguments are up, down, left, right, select. Up/left move to
the previous control; down/right move to the next. Long select goes back.
Configuration is saved in NVS. Expansion input is disabled on first boot.

The onboard expander is polled; it cannot guarantee capture of every edge of a
fast-spinning encoder during a long display or SD operation. Its Gray-code
decoder rejects skipped/bouncing transitions instead of inventing movement.
No external control is physically validated until one is connected and tested.

## Sensors and time

SHTC3 readings are sampled every two seconds with CRC checks. Acquisition is
split across loop iterations; opening Weather does not block for a conversion.
Failed/stale readings are marked unavailable. QMI8658 samples report all three
acceleration axes in g and angular-rate axes in degrees/second. Battery percent
is a voltage estimate, not a fuel-gauge measurement; the board does not expose
a trustworthy charging-state input to this adapter.

The RTC stores UTC; Dragon Detector's saved UTC offset controls local display.
Reads reject oscillator-stop, stopped/test/12-hour modes, malformed BCD and
invalid calendar dates. A successful time setting includes hardware readback.
The RTC requires backup power to retain time with all main power disconnected.

```text
board health
weather status
imu status
clock diagnostics
clock hardware
```

## Reserved buses

| Peripheral | ESP32 pins / address |
|---|---|
| LCD | SCK6, MOSI7, MISO5, DC4, CS8; CH32 EXIO0 reset/PWM backlight |
| microSD | Shared SPI6/7/5, CS9 |
| Shared I2C | SDA27, SCL26 |
| CH32 / RTC / SHTC3 / QMI8658 | 0x24 / 0x51 / 0x70 / 0x6B |
| UART expansion / GPS | ESP RX12, ESP TX11 |
| USB | D+14, D−13 |

Source: [Waveshare board documentation](https://docs.waveshare.com/ESP32-C5-LCD-2.73)
and the bundled Waveshare Arduino 3.3.10 examples/schematic. The vendor source
contains generic EXIO comments for other products; use the board-specific
schematic and EXIO4–14 expansion table for this board.

Native validation: `python tools/test_c5_board.py` runs input safety/debounce
and production RTC functions with simulated I2C faults. Hardware validation
must separately check display, card, sensors and any wired external controls.

## Native display and SD bus

The square panel uses Waveshare's board-specific power/gamma initialization,
RGB666 pixel conversion and 60 MHz ESP-IDF LCD DMA transport. Its address window
is exactly 320 × 320. An eight-row LVGL buffer lives in PSRAM; one 7,680-byte
internal DMA buffer holds converted pixels. LVGL receives completion only after
the transfer finishes, so it cannot reuse an active DMA buffer. The LCD and SD
use the same native SPI2 bus arbiter. SD mounting never formats the card and
reports each result while trying 20 MHz, 4 MHz and 1 MHz.

Explicit diagnostic commands:

```text
c5 display status
c5 display bars
c5 display black
c5 display resume
```

Bars run left to right: red, green, blue, white, black, gray. Black clears the
entire physical panel using the production conversion/transfer path. Both tests
pause ordinary display flushes until `resume`, which invalidates the screen for
a complete redraw. Status reports transfer count, pixels, last/maximum flush
time and failures. `python tools/test_c5_display.py` checks production color
conversion, square/rotated address windows, DMA lifetime and error completion;
physical colors and speed still require a device check.

## Reproducible build and checked flash

```powershell
.\tools\build_piratebox_square.ps1
.\tools\flash_piratebox_square.ps1 -Backup 'C:\path\to\verified-32mb-backup.bin'
.\tools\flash_piratebox_square.ps1 -Backup 'C:\path\to\verified-32mb-backup.bin' -Port COM79 -Execute
```

The build wrapper uses the pinned `waveshare_c5_273` PlatformIO environment,
four jobs, the local RISC-V compiler PATH and UTF-8 Python output. It refuses
to produce a receipt if source or dependency hashes change during compilation.
The resulting `.pio/build/waveshare_c5_273/build-receipt.json` records source,
toolchain and image hashes. A subsequent source change requires another build.
`-ArtifactDirectory` can preserve a separate set of output images.

Without `-Execute`, the flash wrapper checks artifacts and the full 32 MiB
backup plus its `.sha256` sidecar without opening a serial port. With `-Execute`,
it also checks the connected target is ESP32-C5 with 32 MiB physical flash.
It validates both C5 image headers, checks the partition binary against the
reviewed CSV, writes only the bootloader at 0x2000, partition table at 0x8000,
OTA selector at 0xE000, and application at 0x10000, then independently verifies
all four regions. It uses esptool 5.4.x in ROM/no-stub mode. It does not erase
the chip or write a filesystem image.

`python tools/test_piratebox_square_artifacts.py` exercises image-digest,
partition corruption, oversized image and missing/mismatched backup safeguards
with temporary fixtures; it never opens a serial port.

## Future ATGM336H GPS receiver

GPS is optional and disabled initially. The C5 board has no built-in GPS. In
**GPS setup**, select **PRESET ATGM336H** to load RX GPIO12, 9600 baud and 8N1;
selecting a preset preserves the current on/off state. Leave it off until a
receiver is attached. **Generic NMEA** retains manually selected UART settings;
changing the baud returns to the generic preset. Settings persist in NVS.

| GPS connection | C5 connection |
|---|---|
| TX / TXD, 3.3 V logic | J5 pin 1 / GPIO12 / UART RX |
| GND | Common GND |
| RX / RXD | Leave unconnected; firmware receives only |
| Supply | Use the voltage specified for the actual module/breakout |

The manufacturer specifies NMEA0183 at 9600 baud for the
[ATGM336H-5N family](https://icofchina.com/daohang/danpin/2525.html) and
[ATGM336H-5NR32](https://www.hzzkw.com/daohang/danpin/2440.html). Bare modules
require 2.7–3.6 V, typically 3.3 V; a breakout's regulator may change its VIN
requirement. Check the exact board before supplying power. UART signals into
the ESP32 must remain 3.3 V. Other ATGM336H variants or previously reconfigured
receivers may need a different baud, available through Generic NMEA.

Use NMEA RMC and GGA sentences. The transport accepts GN/GP and other NMEA
talker prefixes, validates checksums, and discards stale or interrupted fixes.
No CASIC commands, module baud changes, saved receiver configuration or antenna
power commands are transmitted. Position and RTC synchronization require an
actual fresh fix; no receiver is connected or physically verified yet.

```text
gps external preset atgm336h
gps external status
gps external on 12 9600
gps reception
gps clock sync
gps external off
```

The `preset` command changes the saved host preset without enabling it.
`on 12 9600` explicitly enables the generic NMEA receiver. On the device, enable
the selected ATGM336H preset with the **EXTERNAL GPS: OFF** control after wiring.
Leave the receiver outside with a suitable antenna and a clear sky view for
its first fix. Clock synchronization writes UTC to the board RTC; the saved
display offset controls local time.
