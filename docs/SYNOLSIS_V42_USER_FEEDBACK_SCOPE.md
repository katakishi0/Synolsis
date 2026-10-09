# Synolsis V42 — user screenshots and limited fixes

This is a **test build**, not a claim that every requested emulator subsystem is complete.

## Changes included
- Re-expanded settings navigation into explicit firmware, `prod.keys`, `title.keys`, save data, game folders, and application files, with localized descriptions. Imports reuse existing key and firmware pickers.
- The game-menu edge indicator is theme-colored, only 4dp wide and no longer clickable. A horizontal swipe follows the finger and opens the drawer if it travels at least 72dp. Fix duplicate Android Back callbacks by registering once in onCreate.
- Add details to existing native performance HUD: FPS, process RAM, mean frame time, deviation, and **battery** temperature when reported by Android. This is inspired by Citron's performance information design, **not copied Citron GPL UI code**, and is not a fake GPU or emulation-speed indicator.
- Experimental Frame Skip **Auto** mode 3: after 8 slow host-present operations (>=24ms *excluding guest fence wait*), skip every other display presentation; restore after 16 cheap presents (<=12ms). All guest fences/VI callbacks remain processed. This cannot raise guest FPS or generate frames.
- Make QLauncher validation surface which part of ExeFS is absent (mount / rtld / main / main.npdm), and avoid presenting reinstalling firmware as the sole remedy. No decryption bypass or bundled console keys.

## Explicitly still incomplete
1. Real **QLauncher HOME** services, applet lifecycle, NSO loading and modern firmware compatibility. The new diagnostics only narrow the failure stage. The user screenshot showed the Program NCA was detected but failed ExeFS validation.
2. True guest internal 0.5x/0.75x/1x/2x resolution. The old host swapchain scaling is deliberately pinned at 100% due to Turnip corruption; never advertise it as real native resolution scaling.
3. Frame interpolation / frame generation. Only metadata history and compute shader assets exist, no Vulkan image/synchronization/dispatch path that produces synthetic displayed frames.
4. NVDEC+VIC cutscenes. MediaCodec wrapper alone cannot decode guest NVDEC bitstream and composite VIC images. Need host1x method mapping, SMMU input buffers, H.264 NAL conversion, output format and fence management.
5. Citron-equivalent emulation speed, guest FPS, shader build statistics and thermal graph are not yet integrated. Battery temperature is correctly labeled.

## Device validation
- Run V39 baseline NieR scene and V42 with the **same driver and saved state**, handheld, accurate GPU fence OFF, frame skip 0; compare image and frametimes.
- Check settings keys/firmware cards and locale. Swipe from edge in-game and verify drawer follows the finger in dark/AMOLED palette. Check FPS details.
- Independently enable Auto skip and inspect display smoothness; turn OFF if stutter rises.
- Test installed firmware QLauncher and save precise diagnostic + `emulation.log` (avoid sending prod.keys/title.keys).
- Record a cutscene log to guide future NVDEC/VIC work.
