# Synolsis Emu — real GPU FPS experiments (Strato core)

Source branch: `feature/synolsis-frame-skip-fps-profiles`. Tested target: POCO X5 Pro / Adreno 642L / 6 GB RAM / custom Turnip. User has observed about **24–25 FPS** on the previous build; this is a user-reported reference, **not a measured gain from this patch**.

## Implemented in the native emulator

### Frame skipping (present only)

- UI options: Off (default), skip 1 output frame out of 2, skip 2 out of 3.
- Android stores `presentation_frame_skip` as a true **integer** preference and passes `presentationFrameSkip` via `NativeSettings` and the Android JNI bridge.
- `Settings::presentationFrameSkip` is consumed on the Strato `PresentationEngine::PresentationThread`.
- On skipped output: **wait for the guest producer fence**, invoke `presentCallback`, preserve `vsyncEvent` behavior; omit the Vulkan swapchain acquire/copy/present operations.
- The first guest frame is always presented. Frame skipping **does not bypass guest GPU work, generate new frames, or change game simulation rate**. May save output-copy/presentation overhead at the cost of visible smoothness and accuracy of displayed FPS counters. Default Off.
- Runtime change requires restart of a title for clean A/B comparisons (the native setting can update but frame counter continues).

### Native Vulkan presentation policy

- UI options: Automatic (old Strato behavior), FIFO, MAILBOX, IMMEDIATE.
- `vulkan_present_mode` is persisted as an integer and carried into C++ `Settings::vulkanPresentMode`.
- Selected mode is honored in Vulkan `PresentationEngine::UpdateSwapchain`, validated against the installed driver. Unsupported modes safely fall back to mandatory FIFO.
- MAILBOX can improve latency but may be memory heavier; IMMEDIATE can tear if supported; neither guarantees an FPS improvement. The default Auto follows the existing `disable_frame_throttling` behavior.

### Experimental Adreno FPS Hack profile

- New selectable GPU preset `fps_hack`: 3 executor slots, flush threshold 96, free guest texture data, fast GPU readback enabled, fast readback writes disabled, shader cache enabled.
- This modifies existing, *native-backed* Strato settings. Switching back to other profiles restores both readback shortcuts to off.
- The UI keeps the readback switches and precision selection synchronized. It does **not** make the hardware run at higher clocks or fake frames, and may be slower in some games.

## Other native hot-path optimizations

- `PresentationEngine::PresentFrame` now tracks the most recently applied Android buffer transformation and suppresses repeated `NATIVE_WINDOW_SET_BUFFERS_TRANSFORM` calls if it did not change. The first frame of every new Surface still applies its transform, preventing rotation/surface reattachment regressions.
- `PipelineCacheManager::Run` flushes the staging-cache header when the shader-write queue goes idle or after at most **16** queued bundles. This replaces per-shader header seeks/writes during a burst of shader compilation. At most 15 trailing unindexed shader bundles may be lost if the Android process is force-killed during a burst; cached shaders already committed remain reusable. This is a deliberate disk-I/O versus crash-recovery tradeoff.
- Existing transaction-safe staging append, corrupted-count guard and driver-aware cache identities from the previous build are preserved.
- All these changes reduce overhead in host I/O/presentation but cannot promise larger game simulation FPS on GPU-limited titles.

## Regression/validation

CI validates Java/Kotlin -> native settings field wiring, preference integer types and values, source-backed Vulkan mode and frame skip callbacks, and builds an optimized ARM64 APK. On-device QA still required:
1. Turnip/driver name and version, game version, power profile and scene unchanged.
2. Benchmark at **Off + Automatic**, then test **FPS Hack + Off**, then **MAILBOX (if supported)**.
3. Test frame skipping only last, track *perceived* smoothness and presentation-frame rate separately from game progress.
4. Compare FPS/frame-time, RAM and temperature for 3 minutes per configuration; stop on audio desync, corrupted video, input lag or crashes.
5. Cold shader cache warm-up vs second warm-cache run; do not conflate the warm-cache effect with new FPS hacks.
6. Keep all hacks disabled by default; save copies of game configurations before comparing.

## Future systems — NOT claimed to work

- **True internal resolution scaling** (0.5x/0.75x), upscaling FSR: requires resizing the Switch's guest framebuffer/render-pass/texture allocations and viewport coordinate handling. Merely scaling Android output does not save guest GPU rendering cost.
- **NVDEC hardware decoding**: requires integration with Tegra command submission and host MediaCodec (plus CPU fallback) and synchronization to guest graphics.
- **QLauncher**: Switch HOME Menu system-applet bootstrap/NS/AM/FS/HID IPC is not solved by presenting an Android launcher.
- **Frame generation**: requires optical flow or motion vectors and a host post-processing engine; frame insertion cannot increase actual game simulation FPS and likely consumes too much extra GPU memory on 6 GB Adreno.
- **Full multicore/HLE-service/JIT/shader compiler overhaul**: upstream Eden/Yuzu changes must be audited for architecture and license compatibility and individually regression tested. Unimplemented features must not have decorative UI toggles.

These are independent development efforts, not a one-commit feature flag.
