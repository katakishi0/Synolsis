# Adreno / Turnip performance development plan

_Target platform: POCO X5 Pro 5G, Snapdragon 778G / Adreno 642L, 6 GiB RAM. Date: 2026-10-08._

## Goal and boundaries

Target **Qualcomm Adreno + Vulkan/Turnip only** for first-pass graphics optimizations; avoid spending development time on PowerVR-specific paths. Do not actively break existing standard Vulkan support without a measured technical reason, but test and optimize against Adreno. Preserve Strato/Skyline as the core; screenshots of an Eden-based UI are **feature references**, not license to copy its code blindly.

Test game reported by device owner: NieR:Automata The End of YoRHa Edition, roughly 13 FPS at title menu and on-screen 2745 MB memory indication. One screenshot does not establish average FPS, gameplay performance or actual process PSS/RSS; gather profiling metrics.

## Already supported in Strato source

Verified via upstream code in `strato-emu/strato`:

- `EmulationSettings.kt` and `NativeSettings.kt`: `isDocked`, `systemLanguage`, `systemRegion`, custom `gpuDriver`, `disableShaderCache`, `freeGuestTextureMemory`, `forceMaxGpuClocks`, `forceTripleBuffering`, `disableFrameThrottling`, executor slots/flush threshold and GPU hack toggles.
- `emulation_preferences.xml`: these are exposed through Android settings, including per-game preferences.
- `settings.h` and `android_settings.h`: native bridge to C++ GPU/runtime settings.
- Some shader caches exist; do not mistake a cache hit for eliminating compilation stutter.
- Strato uses native execution on ARM64 (NCE architecture). A new “multicore” toggle cannot safely conjure extra parallelism without profiling scheduling.

## Priorities

| Level | Feature | Engineering target | Verification |
|---|---|---|---|
| P0 | Adreno performance profiles | Presets using existing native-backed settings: Compatible, Balanced, Low-memory, Experimental. Maintain per-game overrides; no placebo toggles. | Verify each setting reaches NativeSettings / GPU; test hot-apply vs restart. |
| P0 | Turnip driver reporting | Detect GPU/driver version and Vulkan capabilities without reading secret files. Expose fallback and error details. | Compare system driver and user-selected Turnip; fail gracefully. |
| P0 | Low-RAM pressure measurements | Enable texture release policies conservatively, collect allocated RAM / Vulkan budget and log OOM, baseline 6 GB handset. | FPS/p99 frame time, memory PSS and crash frequency. |
| P1 | Vulkan resolution scaling | Start with 0.75× and 0.5× for handheld/portable mode, identify framebuffer dimensions and resolve all render-target dependencies. | Image correctness, no corrupted UI/text, measurable FPS delta. |
| P1 | Vulkan present modes | Offer supported FIFO / MAILBOX / IMMEDIATE if available, with correct fallback. Triple buffering already exists. | No 0-FPS lockups, tearing behavior documented. |
| P1 | ASTC hardware preference | Probe Vulkan ASTC features; fall back to CPU when unsupported. | Textures render correctly on Turnip/Adreno. |
| P1 | Upscaling / FSR 1.0 | Implement a shader based on actual scaled output; sharpening isn't useful until resolution scaling works. | Quality vs FPS in screenshots and motion. |
| P2 | Async shader compilation | Pipeline cache dependency and interconnect synchronization must be verified; opt-in with recovery toggle. | Reduced stutter without black textures or hangs. |
| P2 | CPU/GPU frame pacing | Verify present/vsync/choreographer and throttling; distinguish speed limit from real render throughput. | Timestamp traces rather than ungrounded FPS claim. |
| P3 | Video NVDEC hardware path | Complex video decode emulation, possibly MediaCodec bridge; cannot be solved with a UI-only switch. | Video playback clips + service synchronization. |
| P3 | Frame generation / “fast render distance” | Defer; image interpolation does not accelerate underlying game simulation, and draw distance isn't generic emulator property. | Investigate only after native FPS baseline and compatibility are stable. |

## Baseline defaults to evaluate (not auto-applied)

- Portable (`isDocked=false`), caching enabled (`disableShaderCache=false`), Vulkan/Turnip selected via existing driver manager.
- `forceMaxGpuClocks=false` by default due to heat/battery, `validationLayer=false` outside debugging; retain source-specific fallback.
- Keep `freeGuestTextureMemory=true` unless regressions appear; 6 GB device makes memory use important.
- Measure baseline at same location, same game version, same driver and same ambient/thermal conditions, with cache cold and warmed.
- The “fast GPU time”, “auto stub”, and “frame generation” labels in a third-party emulator are not evidence of corresponding safe Strato implementations.

## QA template

Record build commit SHA, Turnip version, Android version, game version, mode (portable/docked), measured average/1%-low FPS, frame-time variance, Android process PSS, estimated Vulkan heap pressure, temperature/throttling indication, and artifacts/crashes. Test original baseline and each candidate branch separately.

## License and provenance

Source references: [Strato emulator](https://github.com/strato-emu/strato). Keep MPL/GPL license headers and attribution on modified files. Do not copy proprietary assets or compile Eden/Yuzu source into the project merely to mimic feature labels.
