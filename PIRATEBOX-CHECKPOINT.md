# Pirate Box C5 source checkpoint

This repository starts from **Pirate Box 0.1.0** for the **Waveshare ESP32-C5-LCD-2.73**: 32 MiB flash, 8 MiB PSRAM, 320 x 320 ILI9488 LCD. PlatformIO environment: `waveshare_c5_273`.

## Provenance

- Saved checkpoint: `waveshare-c5-273/0.1.0-20261101`.
- Original local source commit: `f5d362ae316da002e16e98279481b100c5d0fe38`.
- Shared application base: Dragon Detector 0.13.42, revision `3c18aee5001a88663f7a8fbdad9e6e9b2394cc14`.
- Exact local source archive SHA-256: `5e1e41f7a1541db197083cd8c9080bdc24b0737acd388b07559168e5160d2671`.
- Last device-tested application SHA-256: `3e05c8966a01dcf9728fafeae4010c7de44a963fc88dae83f3b62f930b7cccdb`.

The public repository is a standalone source import. Publication changes are documentation, repository metadata and scanner configuration; firmware and build implementation are preserved. Git may normalize source line endings. The original local archive and build receipt remain unchanged. This does not claim that a fresh rebuild produces byte-identical binaries.

The original source archive, device backup, debug symbols, build receipt and private capture logs remain outside this repository. The public [validation record](docs/PIRATEBOX-VALIDATION.md) retains observations and their limits.

## Saved firmware layout

These hashes identify the previously tested binaries. This source publication does not attach them as a downloadable Pirate Box firmware release.

| Offset | File | SHA-256 |
|---|---|---|
| `0x2000` | `bootloader.bin` | `3eb4d74b15b36d92f47fe2b261a3b76c66bfed4056bfd5e4913d730c5c921317` |
| `0x8000` | `partitions.bin` | `ef087202925d3d22b49879a45c702d04284efc53aa7a1b6e5e55165d79f33769` |
| `0xe000` | `boot_app0.bin` | `f94c5d786a7a8fab06ac5d10e33bf37711a6697636dc037559ea19cc410a17f0` |
| `0x10000` | `firmware.bin` | `3e05c8966a01dcf9728fafeae4010c7de44a963fc88dae83f3b62f930b7cccdb` |

The reviewed bootloader retains its DIO/80 MHz/32 MiB ROM header. Both application slots stay below 16 MiB; the upper 16 MiB is `piratefs`. The four-segment write excludes NVS and both filesystem regions. Firmware initializes only a completely erased upper data volume after its full blank check. See the [board guide](docs/PIRATEBOX-SQUARE.md) before building or flashing.

## Continuing development

Pirate Box has its own pirate artwork, animated avatar, square layouts, launcher startup and button navigation. Dragon Detector is its shared feature base; Dragon Ball/Capsule designs are excluded from the C5 selectable themes.

Add a separate hardware profile and branch for each new board. C5 defaults are KeyL/previous GPIO2, KeyR/next GPIO3 and BOOT/select-and-back GPIO28. Optional ATGM336H GPS uses receive-only GPIO12 at 9600 baud and defaults off. Expansion controls use documented CH32 EXIO inputs and also default off.

External GPS/controls, active RF tools, authenticated uploads and fully disconnected RTC retention require separate acceptance. The November theme sweep and final launcher patch have distinct validation records; see the validation document for scope.

GitHub source publication was authorized on November 20, 2026. Hosting, a web flasher and binary-release publication remain separate future work. Inherited deployment workflows are omitted. Retained `web/` and `hosting/` sources are upstream references, not an enabled Pirate Box service.
