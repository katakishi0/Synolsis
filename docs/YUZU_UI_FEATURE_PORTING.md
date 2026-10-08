# Synolsis Emu — Yuzu/Nyushu-inspired UI and graphics features

**Basis:** self-contained Strato/Skyline core, GPL-sensitive references used only as behavioral and UI inspiration. Date: 2026-10-08. Target: Snapdragon 778G / Adreno 642L / 6 GiB RAM / Turnip.

## Source / legal review

- Yuzu historical Android frontend: https://github.com/liushuyu/yuzu-android (GPLv3-or-later). Modern downstream Eden: https://git.eden-emu.dev/eden-emu/eden, https://github.com/emuall/eden (mostly GPLv3).
- Screenshot-based reference: Nyushu v0.0.6 Android app UI. Source repository not yet independently verified. Screenshots are UI references, **not evidence of transferable implementation**.
- Original Strato sources use MPL-2.0 and original credits. We are **not copying GPL frontend classes, GPL render modules or unverified Nyushu code** into the MPL source tree. A direct Yuzu source port would require a full licensing/attribution review (MPL-2.0 GPL compatibility depends on notices and combined work requirements).
- Preserve Skyline/Strato attribution in LICENSE, third-party credits and native source headers. Rebrand **user-facing emulator product name** to Synolsis Emu; do not falsely claim authorship of upstream components.

## Stage 1 — implemented on `feature/synolsis-next-ui-and-graphics`

1. **Android install package:** applicationId `org.synolsis.emu`. Unlike a re-skin, Android treats this as a **different app**; it does not automatically receive existing saves, keys, firmware or preferences. Make backups via the old app before installing. Kotlin/NDK namespace remains `org.stratoemu.strato` on purpose because JNI C++ symbols and class references use the existing package.
2. **Yuzu-style Android frontend navigation:** original Material3 BottomNavigationView with working Games, Search and Settings tabs using the Strato game list/search/SettingsActivity. It is **not a verbatim copy** of Yuzu/Nyushu UI code or assets.
3. **Adreno real memory/GPU presets:** conservative (3 GPU executor slots, flush threshold 128, free unused guest texture data), balanced (4 slots, threshold 256, free texture data) and experimental (6 slots, threshold 512, keep more textures resident). All profiles keep shader cache enabled and do not force dangerous clock or direct-memory tricks. Settings wired into existing Android preference storage and libskyline/native settings; supports per-game preferences.
4. Previous FW22/23 ZBIC, Vulkan FIFO fallback, staging cache cleanup, driver-isolated pipeline caches, Synolsis color palettes and Experimental toggles are preserved.

### Validation

GitHub Actions `.github/workflows/synolsis-next-ci.yml` checks package ID, XML, exact UI controls, profile wiring, and compiles `:app:assembleMainlineDebug` (unlike the earlier `devDebug`, this APK gets exact package `org.synolsis.emu`). No claim of runtime stability or higher FPS until tested.

## Stage 2 — active engineering backlog; not yet implemented

| User-requested feature | Baseline/actual mechanism | Work required before working toggle |
|---|---|---|
| Nintendo Switch **QLauncher** / HOME Menu | System applet ID `0100000000001000`; Strato has some `am` system applet proxy skeleton, but not a booted HOME Menu | Discover/install authorized qlaunch NCA via firmware, implement NS applet launch and process classes, required appletAE/NS/HID/AM/VI IPC, correct applet bootstrap and process switching; test errors before enabling UI |
| **Frame skipping** | No generic skip-present option in current Strato | Implement safely at the renderer/presentation scheduler with Vulkan semaphores, guest timing, audio continuity and frame pacing; avoid skipped frame corruptions |
| **Frame generation** | No interpolator/motion vectors/frame history | New post-processing/optical-flow pipeline, extra GPU RAM/time and latency controls. On 13–16 actual FPS this will **not** increase game simulation speed; opt-in only after real profiling |
| **Multicore** | Strato has NCE host CPU and parallel GPU executor; the profile selector controls real executor work | Profile CPU/GPU contention and thread affinities; do not advertise imaginary CPU cores |
| **Fast Render Distance** | Geometry draw distance is often defined by the game | Requires title-specific hacks or graphics heuristics; cannot be universally turned down |
| **Fast GPU Time** | Guest timing queries, GPU fences and clocks | Implement calibrated time-query overrides only per-title; values too aggressive can break dynamic resolution and pacing |
| **GPU precision** | Fast readback and subgroup handling switches exist | Map exact options to safe/accurate/experimental mode after game regressions |
| **Shader Backend GLSL / SPIR-V** | Native Strato Vulkan SPIR-V route, no drop-in OpenGL renderer | Maintain Vulkan first; shader IR emission GLSL needs actual compiler path and compatibility/driver tests, not simple UI selector |
| **NVDEC emulation GPU/CPU** | NVDRV services plus codecs exist, but do not prove Android hardware video decode | Inspect Tegra VIC/NVDEC command processor, implement optional MediaCodec hardware decoder with CPU fallback, synchronize surfaces/audio |
| **VRAM/Memory usage modes** | The first phase profiles existing texture disposal and GPU executor queue memory | Next: Vulkan heap budget, paging/eviction policy, MB counters, OOM recovery; cannot invent dedicated physical VRAM on shared-memory phones |
| **Resolution 0.5×/0.75×/1×**, dynamic resolution | Requires internal render-target scale and guest coordinate/viewport management | Introduce scaled internal surfaces with framebuffer copies, proper postprocessing, text UI correctness. FSR then follows. |
| **VSync/PRESENT modes** | Existing triple buffering and disable-frame-throttling + new FIFO fallback | Test FIFO/MAILBOX/IMMEDIATE with driver support queries, safe fallbacks and audio pacing |
| **Thermal/Adreno Turnip tuning** | 6GB POCO X5 Pro, GPU Adreno 642L, custom Turnip | Android thermal API telemetry and p95 frame time, avoid forcing max clocks by default |

### Benchmark protocol

Using the **same** Turnip, title update, game scene, power profile, ambient conditions and shader cache warm state, record at least 3 runs of 2–5 minutes each. Compare avg/p1% FPS, p95 frametime, peak RAM/PSS, GPU heap budget and crash rate. An isolated title screen showing 16 FPS is not proof of sustained gains. Do not add a switch that does not change the actual emulator.

## QLauncher source facts

Switchbrew: https://switchbrew.org/wiki/Qlaunch and https://switchbrew.org/wiki/AM_services. Program ID `0100000000001000` is a privileged system applet requiring special services and launch parameters. The presence of firmware archives is **not** sufficient to boot it. We will not distribute proprietary firmware, keys or a copy of Nintendo's qlaunch.

## Package ID / saving data warning

**Old `org.stratoemu.strato.dev` debug APK and new `org.synolsis.emu` mainline APK have separate Android app sandboxes**. Before installing the new application, export/save any needed save data and record your Turnip driver, game directories, settings and legally obtained keys/firmware location. Never uninstall the old app expecting Android to migrate its data automatically.
