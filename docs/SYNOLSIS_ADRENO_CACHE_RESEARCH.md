# Synolsis Emu — Adreno/Turnip cache and theme experiment

**Branch:** `feature/synolsis-themes-cache`. Core: native Skyline/Strato, unchanged application ID, but an APK can upgrade an existing installation **only with the same signing certificate** (CI debug APKs may differ). Back up saves before any reinstall. Source and original author/license headers preserved.

**Hardware under evaluation:** POCO X5 Pro 5G, Snapdragon 778G/Adreno 642L, 6 GiB RAM, user-provided Turnip driver.

## Sources inspected

- [Pine v2.1.0 release changelog](https://newreleases.io/project/github/Ishan09811/pine/release/2.1.0): fallback to FIFO when a present mode is unsupported, swapchain image count guard, memory-manager heap limits, async shaders, UI fixes. Current directly verifiable Pine source repository unavailable; don't copy unknown patch bytes or promise async shader support.
- [Pine v2.0.0 release changelog](https://newreleases.io/project/github/Ishan09811/pine/release/2.0.0): theme switching, Vulkan headers, shader replacement file loading, VSync controls, NDK and memory optimizations. Features need actual Strato implementation, not copied toggles.
- [Eden #4496](https://git.eden-emu.dev/eden-emu/eden/pulls/4496): correct shader cache path when switching GPU drivers. Different project structure; concept of not reusing stale caches is relevant to Strato.
- [Eden #3953](https://git.eden-emu.dev/eden-emu/eden/pulls/3953): Vulkan performance regressions from universal shader workarounds. Do not apply generic fixes to all GPUs without profiling.
- [Eden #4234](https://git.eden-emu.dev/eden-emu/eden/pulls/4234): experimental VMA/garbage collector rewrite and memory reports; closed/unmerged, high risk. **Not imported.**
- [Eden #4362](https://git.eden-emu.dev/eden-emu/eden/pulls/4362): sparse buffers and mapping correctness; requires substantially different cache architecture; **not imported**.
- [STORM_SWITCH](https://github.com/ReiKatari/STORM_SWITCH): performance/driver/UI goals; avoid unverifiable FPS claims without independently reproducible source diffs and tests.

## Implemented in this native Strato fork

1. **Synolsis Emu rebranding**: launcher label, main header and original vector adaptive icon. Keep original Skyline/Strato attributions and unchanged Java application package/native library IDs to preserve compatibility.
2. **Selectable palettes**: Original/Material You, Midnight Blue, Neon Purple, Emerald Green, AMOLED Red and Solar Amber. Five additional colors override Material 3 surfaces, backgrounds, primary/secondary accent and text on all activities before creation. Restarts the frontend after switching palettes.
3. **Staging cache replay fix** in `app/src/main/cpp/skyline/gpu/pipeline_cache_manager.cpp`: validate that complete headers were read, refuse invalid main/staging streams, flush a successful staging merge and remove merged staging data so previous pipelines aren't appended again every boot. Retain staging for recovery on an I/O failure; never overwrite key/game files.
4. **Driver-separated Vulkan caches** in `app/src/main/cpp/skyline/gpu.cpp`: namespace both graphics pipeline cache types using a stable hash of the selected driver label. Prevents using Turnip/stock driver cache data together; on first use of a given driver, expect cold cache and temporary stutter.
5. **Vulkan swapchain negotiation hardening** in `app/src/main/cpp/skyline/gpu/presentation_engine.cpp`: clamp triple-buffer requests to supported surface maximum, fall back to Vulkan FIFO when MAILBOX is unavailable rather than failing launch.
6. Existing experimental native-backed Adreno switches from earlier work remain inline, and the FW22/FW23 ZBIC loader branch is preserved.

## What is *not* done

- Async shader compilation, real resolution scaling, frame generation, FSR, global memory allocator rewrite and CPU optimization require separate implementation and real regression testing. Not implied by these changes.
- Fixes above target cache reliability and Vulkan startup compatibility; they are **not an FPS unlock**. The user saw 16 FPS in a Pine screenshot versus 13 FPS earlier. The two screenshots do not prove Synolsis speedup, because they may be different emulators, settings and conditions.
- Shader cache data is not shareable across games, proprietary firmware files, arbitrary drivers, or shader compiler revisions. Always test after clean/warm cache and compare like-for-like.

## Validation

CI: [Synolsis Themes + Cache](../.github/workflows/synolsis-themes-cache-ci.yml). It checks theme resources, native cache source invariants and attempts ARM64 APK build. Device QA: same Turnip version and game, same scene for 3–5 minutes, note FPS median/p95 frame time, warm-cache stutters, peak memory/PSS, crashes and display corruption. Run cold-cache once and warm-cache again. Do not upload commercial ROMs, keys or firmware.

**Release gate:** Kotlin/C++ compilation success, APK artifact, frontend palette switching test, per-game settings and Vulkan graphics stability test. No release promise until all pass.
