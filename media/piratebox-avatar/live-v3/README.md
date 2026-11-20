# Friendly live mascot, version 3

The user requested more poses, a happy expression, and a live radar mascot that reacts to detection families. The original supplied avatar is `../source-avatar.png`. Its bandanna, glasses, facial hair, outfit and red chest mark are the identity reference; changing the former frown was explicitly requested.

`avatar-sheet.png` is the final transparent sixteen-frame sheet created with the built-in imagegen tool. `avatar-prompt.txt` and `spacing-prompt.txt` preserve both prompts. `avatar-sheet-initial.png` preserves the first result; the second pass added transparent gutters and removed floating symbols. No CLI/API fallback was used. The image generator's native files remain under the Codex generated-images directory, and these project copies are the build inputs.

The mechanical packer is `tools/pack_piratebox_live.ps1`. It selects the sixteen cells at reviewed transparent gutter coordinates, crops to visible alpha bounds, samples nearest pixels at a common scale and baseline, preserves sampled alpha, and encodes swapped RGB565 plus alpha. It does not redraw the artwork. It rejects any cell whose visible pixels touch a crop boundary.

| Frames | Pose | Live use |
|---|---|---|
| 0, 1, 2, 3 | Smile, blink, breathe, wave | Calm idle loop |
| 4, 5 | Curious, small head tilt | General curiosity / tracker |
| 6, 7 | Point | Wi-Fi encounter |
| 8, 9 | Listen to radio | Bluetooth encounter |
| 10, 11 | Excited open-hand greeting | Drone encounter |
| 12, 13 | Thoughtful | Camera encounter |
| 14, 15 | Wide-eyed attentive greeting | Alert encounter |

The firmware embeds four 145 × 180 large frames, sixteen 48 × 64 live frames and the unchanged 260 × 95 title: **534,756 bytes** of image payload, 147,456 more than version 2. The happy stock badge adds 55,720 bytes of RLE. Images remain in flash; each live widget allocates only its small animation state and normal LVGL objects. No SD file is required to animate.

Idle breathing changes every 1.2 seconds, the blink lasts 140 ms, and the wave lasts 540 ms once per 8.4-second loop. Each reaction alternates only its own two frames every 360 ms for 2.52 seconds, then returns to idle. Boot, menu and watch accessors share the calm large-frame timing. Changing image sources preserves the LVGL image pivot, and nearest-pixel scaling remains enabled.

`pirateboxMascotShow(parent,x,y,maxHeight)` returns an image with its own state, animation and delete callback. `pirateboxMascotReact(image,reaction)` has no global widget pointer. The delete callback cancels animation before freeing the state. UI callers clear their own image pointer when cleaning a page. `pirateboxAvatarIdleFrame(elapsedMs)` exposes the same calm timing for manually drawn watch faces.

`packed/` contains RGB565+alpha payloads and PNGs reconstructed from those exact bytes. `mascot-packed-preview.png` enlarges those pixels threefold without smoothing; `boot-packed-preview.png` shows the four exact 320 × 320 boot compositions. Both were inspected after packing. `packed/piratebox-stock-happy-320.ddb` is the optional static happy badge; the SD bundle uses the new filename `/dragon/resources/piratebox-avatar-happy-320.ddb` so installation does not overwrite an older card file.

RISC-V syntax validation uses the actual C5 build headers. `tools/test_piratebox_mascot.py` builds the actual implementation against the bundled LVGL and checks repeated page creation/deletion, independent mascot states, reaction pairs and expiry, image pivot preservation, foreign/deleted pointer handling, and millisecond counter wrap. Physical display inspection is a separate integration step.
