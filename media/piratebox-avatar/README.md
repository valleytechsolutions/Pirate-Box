# Pirate Box avatar boot artwork

**Current firmware uses [the friendly sixteen-frame live-v3 artwork](live-v3/README.md).** Repack with `./tools/pack_piratebox_live.ps1`. It provides calm blinking/breathing/waving, six family reaction pairs, four smiling large frames, and a matching happy stock badge. The version-2 files and description below remain as provenance; they are no longer the current embedded avatar.

The owner supplied `source-avatar.png` and requested an animated pixel-art avatar with the words **PIRATE BOX**. The four-frame sprite sheet and gold/red pirate title were generated with the built-in imagegen tool. The exact prompts are retained in `avatar-prompt.txt` and `title-prompt.txt`; no API/CLI fallback was used.

- `pirate-avatar-sprites.png`: original generated transparent 2 × 2 sprite sheet, 1254 × 1254 pixels.
- `pirate-box-title.png`: original generated transparent title, 2079 × 756 pixels.
- `packed/avatar-0.png` through `avatar-3.png`: 145 × 180 previews reconstructed from firmware RGB565+alpha bytes.
- `packed/title.png`: 260 × 95 preview reconstructed from firmware bytes.
- `packed/boot-frame-0.png` through `boot-frame-3.png`: exact 320 × 320 placement previews on black.
- `packed/piratebox-stock-320.ddb`: static square badge matching the first boot frame.
- `boot-packed-preview.png`: four-frame contact sheet for review.
- `SHA256SUMS.txt`: source, prompt, and generated payload hashes.

Repack from the repository root with PowerShell 7 on Windows:

```powershell
./tools/pack_piratebox_live.ps1
```

The packer only selects sprite cells, excludes near-invisible outer alpha specks from crop bounds, samples with nearest-neighbor scaling, and encodes the existing artwork. It does not redraw the character or lettering. It retains each selected pixel's alpha and quantizes color to swapped RGB565, matching `LV_COLOR_DEPTH=16` and `LV_COLOR_16_SWAP=1`. The boot include occupies 387,300 bytes of firmware image data. No SD file, decompression buffer, or runtime image allocation is needed for the boot animation. The static badge adds a 56,820-byte RGB565 RLE payload; the packer validates every decoded badge pixel against the first packed boot preview.

`pirateboxBootShow(parent)` creates two LVGL image objects and an animation attached directly to the avatar child. Each frame lasts approximately 180 ms. LVGL's normal object destructor deletes its animations when the parent/page is removed, so there are no retained widget pointers or timers that can reference a deleted page. `pirateboxBootTick()` is intentionally empty; the existing main `lv_timer_handler()` drives animation.

The original generated sheet and title were visually inspected, including correct title spelling, consistent avatar identity and outfit, and transparency. The 320 × 320 packed previews were inspected after RGB565 encoding. `piratebox_boot.cpp` passed strict C++ syntax validation against the repository's actual LVGL headers with `-Wall -Wextra -Werror`; the complete C5 build and physical display/boot validation are separate integration steps.
