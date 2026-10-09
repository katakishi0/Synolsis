# Synolsis V33 — GPU presentation and Host1x/NVDEC safety checkpoint

Base: Synolsis V32 SVC SignalEvent + Host1x command bounds checks, itself based on V31 Vulkan WSI and V30 Maxwell/SVC/MediaCodec safety.

## Changes

- Fix **actual NDK compilation error** in the V31/V32 surface format fallback: `format::R8G8B8A8Unorm` / BGRA are `FormatBase` values and must use `.vkFormat`, not `*format::...`.
- Treat `vkAcquireNextImageKHR` returning `VK_SUBOPTIMAL_KHR` as an already-acquired image; do not reacquire on the same binary semaphore. This avoids invalid synchronization and possible corruption or infinite acquire loops.
- Before replacing an existing Vulkan swapchain, wait for device idle, retire old frame fences and reset frame index; this only affects recreation, not every frame.
- Rate-limit unsupported VIC register logs to avoid logcat spam on cutscene/video paths, like V30 did for NVDEC.
- Return NVHOST guest errors rather than throwing when a channel lacks a valid syncpoint.
- Includes previous V32 bounds-checked Host1x method words, index checks, Horizon SVC 0x11 SignalEvent, and V30 portable default.

## Still incomplete

The original Nintendo Switch NVDEC guest command interpreter, access-unit extraction from SMMU, complete H.264/VP9/VC1 decoding, VIC color conversion and GPU fences are not integrated. Full SVC/HOS, QLauncher, internal resolution 0.5x/0.75x and GPU frame generation are not complete. Build pass does not mean Nine Sols/NieR flicker fixed.

## On-device test

1. Backup saves and preserve driver/settings. Use handheld, Turnip 26.3.0 R6, Frame Skip 0, pipeline cache on, FIFO/Auto, output scale 100%, no fast readback hacks.
2. Reproduce same NieR/Nine Sols scene for three minutes: compare V29 original renderer vs V30 vs V33. Screenshot/record black or magenta blocks, repeated small images, FPS/frametime, RAM and any freezes.
3. Attach logs containing `Swapchain WSI:` and Vulkan/device errors if possible. Note game version/update/DLC and firmware version (not keys).
4. If graphics regress, revert to V30 or V29 and report exactly which version and driver.
