# Pirate Box C5 validation — 2026-11-01

This record separates physical observations on the connected Waveshare
ESP32-C5-LCD-2.73 from host tests and inherited feature availability.

## Physically observed

- The user confirmed correct colors, black levels and motion after the native
  ILI9488 DMA driver replaced the earlier display path.
- After the USB backpressure and input fixes, the user confirmed all three
  buttons work. Raw GPIO readings and separate physical counters recorded
  KeyL/previous, KeyR/next, BOOT click/Select and held BOOT/Back. The pins remain
  GPIO2, GPIO3 and GPIO28 respectively.
- The six launcher entries opened their intended pages through the same
  navigation interface: Radar, Weather, BLE Lab, Attacks, Tools and Settings.
- After relocating three application tables to PSRAM, internal free heap with
  Wi-Fi and BLE initialized increased from about 15 KiB to about 40 KiB.
  Contact, alert and beeper capacities were retained.
- A closed-host test confirmed the application continued running for 30 seconds
  without a serial reader. Normal diagnostics no longer wait on USB output.
- SD mounted through native SPI2, and all nine USB-installable bundle files
  passed complete SHA-256 readback. The extended vendor registry loaded 54,282
  entries. The newer happy-avatar archive was added under a different filename;
  existing card data was retained.
- BLE Lab loaded the bundled five-attribute profile from SD. Comparing it with
  the built-in synthetic demo reported zero added, removed or changed entries.
  This did not connect to, emulate or control another Bluetooth device.
- SHTC3 temperature/humidity and QMI8658 motion readings were valid, with zero
  reported sensor errors during the recorded checks.
- RTC UTC set/read/ticking checks passed, followed by an accurate hardware time
  read after a software restart. An earlier oscillator-stop indication remains
  unexplained; validation still rejects it. No flag was masked or oscillator
  setting changed. Fully disconnected power retention was not tested.
- Seventeen saved history identities survived a restart on the upper 16 MiB
  `piratefs` volume. The old lower filesystem's 6,160,384 bytes independently
  matched the preserved backup through the ESP32 ROM verification operation.
- Passive CSI received 143 accepted C5 106-byte LLTF samples, with zero queue
  drops, and Stop restored radio ownership. The observed AP provided only five
  usable baseline windows; calibration quality/classification is not validated
  by this test.
- The external GPS preset was saved as ATGM336H, GPIO12 RX, 9600 baud, disabled.
  The receiver is not attached, so no GPS fix or external wiring is claimed.

The supporting local logs and screenshots are in `.work/live-first-boot`,
`.work/physical-buttons-live.log`, `.work/live-navigation-rtc-2`,
`.work/sd-install-happy-retry.json`, `.work/csi-device`,
`.work/retention-before`, `.work/retention-after-3`, and
`.work/legacy-fs-preservation.log`. Network observations in those development
logs are not release assets.

## Host checks

The C5 build uses the isolated pinned toolchain and produces a receipt of source,
dependency and image hashes. Flashing checks the retained 32 MiB backup, physical
target/flash size and all four written firmware segments independently.

Production-code tests cover display color/window/DMA lifetime, physical input
debounce and mappings, RTC invalid-data/I2C failures, memory allocation failure
and constructors, NimBLE external allocation, the blank-only filesystem policy,
bounded detection-feed behavior, exhaustive clock/theme mapping, GPS presets,
SD resource parsing, and CSI setup/restoration faults. The actual bundled LVGL
runtime checks mascot and input lifetimes, stale-click cancellation and semantic
focus restoration during dynamic BLE Lab redraws.

Active Wi-Fi/Bluetooth tool operations, authenticated service uploads, external
GPS and wired expansion controls need their own hardware acceptance. Compiling
or exposing an inherited tool does not establish its radio behavior on C5.

## Final menu revision

The square menu revision was built and flashed with all four segments
independently verified. Six launcher entries and six Settings categories opened
through the C5 input path. Forty-three on-device screen captures covered the
launcher, Settings, time/date editors, themes, BLE Lab, Attacks, GPS, Wardrive,
targets, history, SD files, notebook and other selection pages. The menu revision
uses full-width wrapped rows, native pagination/action bars and fixed Home/Back
controls. Lab redraws retain exact device/profile/control focus when it is still
valid.

All **43 selectable built-in themes** passed the previously crashing Themes
page followed by their matching clock face. The runtime catalog excluded the
Dragon/Gold/Capsule choices and the unavailable Personal theme. There were no
recorded failures or resets across 119 health samples; the user's Ghostbusters
theme was restored afterward. Evidence: `.work/square-release-themes/report.json`
and its clock screenshots.

The production input implementation was tested with actual LVGL for text-only
reading controls, bounded scrolling, ordinary row/roller behavior, object
deletion and 100 rebuilds with stale Select cancellation. The device then entered
Radio Health reading mode, scrolled to the final explanatory lines and back to
the top, exited reading, and selected Home. The diagnostic panel follows its
text height, so its bottom is not an empty fixed-height tail. Evidence:
`.work/c5-reading-test/result.txt`, `.work/square-release-device/report.json` and
the top/bottom screenshots. These checks inject the same navigation events as
the board input layer; the earlier user confirmation remains the physical key
acceptance.

The header uses compact live avatar frames. Pixel inspection found the avatar
in all ten relevant captures in `.work/square-final-device`. Earlier visual
interpretations of blank avatars were incorrect; the saved older images also
contain the character. No intermittent rendering failure is claimed from those
captures.

The final patch changes two Settings labels and guarantees launcher startup
after badge uploads, including when a legacy badge-startup flag exists. It does
not change the tested themes, clock rendering, display transport, input mapping
or reading controls. Its boot/navigation acceptance and final artifact hashes
are recorded in the packaged validation summary. External GPS/controls,
authenticated uploads, active RF operations and fully disconnected RTC
retention retain the limits stated above.

Final startup acceptance passed on the last flashed image: the prior image
saved `boot=1` through the shared badge command, then the new image started on
Menu with zero input actions. Its effective badge boot status was zero; the
legacy preference was returned to its original off state afterward. No custom
badge file was present during this startup test, and its checksum remained
unchanged. Settings opened with the buttons, **Open radar** opened Radar, the
Ghostbusters clock remained selected, and Home was left displayed. RTC time
was still accurate without another sync; SD, weather and IMU were ready, and
the display reported zero errors. Evidence: `.work/launcher-startup-prepare`,
`.work/launcher-startup-boot.log`, `.work/launcher-startup-final` and
`.work-pirate-square-final4-flash.log`.

The final `firmware.bin` SHA-256 is
`3e05c8966a01dcf9728fafeae4010c7de44a963fc88dae83f3b62f930b7cccdb`.
The build receipt records the source/dependency hashes and all four image
hashes. The package deliberately omits the unreviewed merged factory image;
the verified four-segment write retains NVS and the existing filesystems.
