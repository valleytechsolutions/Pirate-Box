# Pirate Box SD resources

The square firmware uses Dragon Detector 0.13.42's actual SD formats. The staged [sdcard directory](../sdcard) contains the complete existing upstream resource archive plus Pirate Box documentation and a BLE Lab demo. [sdcard-manifest.json](../sdcard-manifest.json) records each size, SHA-256 and installation purpose. No credentials, captured device identities, user recordings or previous configuration are included.

## What belongs on the card

| Resource | Location | Behavior |
|---|---|---|
| Extended IEEE vendor registry | `/dragon/resources/vendors.ddr` | Loaded once at startup into PSRAM; 54,282 entries, snapshot 20261023, 1,270,072 bytes. |
| Registry provenance | `/dragon/resources/manifest.json`, `research-notes.txt` | Exact original metadata from the upstream archive. |
| BLE Lab demo profile | `/dragon/profiles/piratebox-demo.ddg` | The firmware's own synthetic demo serialized as DDG1 with its CRC; five attributes. Loading it does not start BLE operations. |
| Happy avatar badge archive | `/dragon/resources/piratebox-avatar-happy-320.ddb` | Optional 320 × 320 DDB1 copy. It is not automatically activated; the same happy image is embedded in firmware. Its new filename preserves older files already on a card. |
| Setup and inventory | `/dragon/resources/ATGM336H-SETUP.txt`, `FEATURES-AND-STORAGE-20261101.txt`, `UPSTREAM-READ-ME.txt` | Wiring, storage roles and original resource-pack instructions. |
| Blank upload configuration example | `/dragon/resources/wardriver-config-example.txt`; card root `wardriver.cfg.example` | No credentials. The actual private file is `/wardriver.cfg`, which the installer preserves. |

The original `dragon-resources-v1-20261023.zip` is retained in `web/resources/`. Its SHA-256 is `e2fff4498958e5e8b0d266b83a6ef3c0edc431df72cc295ae4e632607f6a2d2b`. All four archive members are included unchanged, including the card-root `READ-ME.txt`. The registry file's SHA-256 is `66c709f56bc8d3c28f491003949a49c6a73958b50c9b66bcc51cd9dadf7c4bbe`.

## Features already embedded

The C5's 43 selectable built-in themes, fonts, icons, radar artwork, clock faces, Pirate Box boot animation and reactive mascot are compiled into flash. They do not need a separate SD artwork or theme download. Choose a theme under **Settings → Look & Feel → Themes & radar**. A Personal theme appears there after a valid upload; its data lives in internal LittleFS at `/personal/active.ddtheme`. The active custom badge lives at `/badges/active.rgb565`. Their dedicated upload protocols remain available and this installer leaves both untouched. Dragon Ball and Capsule theme choices are excluded from Pirate Box.

BLE service UUID names, manufacturer IDs, advertisement decoders, HID/report parsing, supported tracker protocols and BLE Lab operations are embedded. This firmware has no external GATT-service dictionary reader or downloadable BLE feature modules. Actual peripheral profiles come from discovery and carry the device's real service/characteristic tree, properties and values. The bundled `piratebox-demo.ddg` is the existing **Dragon Lab Demo**, with Battery service/read-notify characteristic and a custom read/write/notify loopback service/characteristic plus descriptor. It is a synthetic workspace example, not an impersonated device capture. Use **BLE Lab → Saved profiles** or `lab load piratebox-demo` to inspect it. User profiles and events remain intact.

The C5 `ReconFS` adapter stores profiles and research output below SD `/dragon` when the card is mounted, with internal-storage fallback when absent. Storage initialization creates `captures`, `profiles`, `events`, `surveys`, `missions`, `wardrive` and `resources` directories. Session folders are created when needed. These are outputs, so empty captures or placeholder event logs are not included in the bundle.

Additional optional inputs are user-created SSID lists at `/dragon/lists/ssids.txt`, saved recipes at `/dragon/recipes/<name>.txt`, and a custom portal template at `/dragon/portal/template.ddh`. Their respective commands create directories and validate content. The portal already includes a built-in local test page; none of these custom files is required for the firmware to start or expose its tools. The bundle preserves existing choices and does not install SSIDs, automated radio recipes or a custom portal. Internal data uses the independent upper-flash `piratefs` volume described in the [square guide](PIRATEBOX-SQUARE.md).

Sensor/RTC/weather code and receive-only NMEA GPS support are embedded. Use **Settings → Radio & GPS → GPS setup** for the optional receiver, **Settings → Time & Date** for the clock and **Settings → Controls** for expansion input modes. ATGM336H setup needs no downloadable GPS binary or map database. External GPS, controls and inherited radio workflows still require their respective physical validation. Old Marauder-derived `.pba`, `.pbb` and `palettes.json` assets have no consumer in this firmware and are omitted.

## Verify and install through USB

Use the private Python environment provisioned by the [build wrapper](../tools/build_piratebox_square.ps1). Close other serial monitors first. These examples use the currently connected board's port; supply the correct port on another computer.

```powershell
$pbPython = "$env:LOCALAPPDATA\PirateBoxSquare\venv\Scripts\python.exe"

# Local manifest/hash validation; does not open a device.
& $pbPython tools/install_piratebox_square_sd.py

# Read-only inventory and full SHA-256 readback of existing bundle files.
& $pbPython tools/install_piratebox_square_sd.py --port COM79

# Add only missing files and read each back to verify its SHA-256.
& $pbPython tools/install_piratebox_square_sd.py --port COM79 --execute
```

The installer checks the square Pirate Box identity, mounted SD and inactive tool ownership before transferring. It uses the existing `PUT_READY` / cumulative `PUT_ACK` protocol with small paced writes. Firmware writes a new `.part` file and renames it after completion. The host then retrieves the complete file with `get` and compares SHA-256. Re-running resumes at file boundaries and reuses files whose hashes match.

Existing differing files and `.part` files are reported and preserved. There is no replace, delete or card-format option. A conflict requires an explicit separate decision after inspecting or backing up the existing file. The installer never opens private `.cfg` paths, sends reset commands, starts radio tools or changes saved Personal themes/badges. Root documentation has equivalent USB-accessible copies under `/dragon/resources`; the root `wardriver.cfg.example` remains available in the removable-card bundle. Device JSON reports are saved under `.work/sd-resources-*.json` or the explicit `--report` path. Exit code 0 means all public USB bundle files matched; 2 means missing/conflicting/partial files; 1 means a transport or validation error.

Restart after first installation of `vendors.ddr`, then use:

```text
resources status
lab list
lab load piratebox-demo
lab status
```

Expected registry fields are `status=ready schema=1 snapshot=20261023 entries=54282 bytes=1270072`. Upload/readback alone does not prove the runtime loader accepted the pack. `lab load` only changes the local workspace; no clone, bridge, connection or advertisement command is sent by the installer.

## Removable card and packaging

Copy the **contents** of `sdcard/` to a FAT/FAT32 card's root, preserving existing files and directories. Do not copy a containing `sdcard` directory. The active private configuration is `/wardriver.cfg`; firmware creates a blank file only if one is absent. Add your credentials locally only when configuring authenticated uploads.

The package tool validates all staged hashes, compares every upstream ZIP member byte-for-byte, and compiles a host utility against the actual firmware `ResourceIndex` and BLE profile headers. That utility validates the registry and regenerates the exact demo for comparison. It then creates a deterministic ZIP; no serial device is opened.

```powershell
& $pbPython tools/install_piratebox_square_sd.py --self-test
& $pbPython tools/package_piratebox_square_sd.py
# Optional: --compiler C:\path\to\host\g++.exe --output C:\path\to\bundle.zip
```

Protocol tests exercise fragmented binary/header reads, cumulative duplicate ACKs, read-only behavior, matching-file reuse, conflict/partial preservation and corrupted-readback rejection. They complement the on-device SHA-256 checks; they do not replace physical SD, radio or sensor testing.

Source consumers: [`sd_resources.cpp`](../firmware/src/sd_resources.cpp), [`resource_index.h`](../firmware/include/resource_index.h), [`ble_profile.h`](../firmware/include/ble_profile.h), [`ble_lab.cpp`](../firmware/src/ble_lab.cpp), [`recon_storage.h`](../firmware/include/recon_storage.h), [`storage.cpp`](../firmware/src/storage.cpp), [`main.cpp`](../firmware/src/main.cpp), [`badge.cpp`](../firmware/src/badge.cpp) and [`personal_theme.cpp`](../firmware/src/personal_theme.cpp).
