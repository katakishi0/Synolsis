# Synolsis Emu — Adreno/Turnip performance experiment v2

**Hardware:** POCO X5 Pro 5G; Snapdragon 778G, Adreno 642L, 6 GB RAM, custom Turnip Vulkan (Stable Turnip v2). **OS target:** Android/MIUI/HyperOS. **Date:** 2026-10-08.

## User-provided initial observation

The latest screenshot of NieR:Automata's **title/menu screen** shows **23 FPS and 2,413 MB**. Earlier screenshots showed 13 and 16 FPS, but these are isolated moments and may use different emulators, cache warmness, title-screen animations and other options. This is **not** evidence that any specific v2 patch increased FPS.

Screenshot settings: GPU/memory conservative preset (executor slots 3, flush threshold 128, free unused guest texture memory ON), GPU clocks OFF, Direct Memory Import disabled for Turnip, Fast GPU Readback ON and Fast Readback Writes ON, Disable Subgroup Shuffle OFF. Selected driver in screenshot: Stable Turnip v2.

**Risk:** Fast Readback Writes can cause graphical corruption. When comparing, disable that switch alone and restart the game. This may sacrifice throughput for correctness.

## Changes implemented in v2

1. **MIUI refresh-rate target fixed:** `EmulationActivity.force60HzRefreshRate()` now sends its actual Android `packageName` (`org.synolsis.emu`) to MIUI's power keeper. Old Skyline's hardcoded `skyline.emu` no longer matches our app. This is a refresh/pacing compatibility fix, not a raw renderer optimization.
2. **Detailed native frametime display:** enable `Exibir estatísticas de desempenho`, then `Mostrar frametime detalhado`. Displays FPS, RSS MB, average frametime (ms) and deviation (ms) already exposed by native JNI. Defaults to OFF to preserve the compact HUD.
3. **HUD polling lifecycle and overhead:** reduce frontend stats polling from 250 ms to 500 ms, and stop rescheduling after the Activity is finishing/destroyed. This reduces unnecessary main-thread and `/proc` work.
4. **Readback profile coherence:** the displayed precision preset now reflects the two real readback flags, including when toggled separately. Disabling GPU readback clears Fast Readback Writes, avoiding stale unsafe state. Per-game and global settings both use the same code path.
5. **Pipeline cache corruption check:** check read byte count, version and magic before replay; reject impossible entry counts that exceed payload bytes. This is a startup reliability fix, not a new shader compiler.
6. **Optimized native APK variant:** `assembleMainlineOptimized` builds the emulator core with CMake Release flags, including `-Ofast -flto=full`, while retaining the same `org.synolsis.emu` package, debug key, no resource shrink/R8 and in-process emulation. Prior `assembleMainlineDebug` compiled the main Skyline core at `-O0`. Compare these two APKs on the phone; this may be the strongest performance lever but carries codegen/fast-math regression risks.

## CI and download

The single maintained APK workflow `.github/workflows/synolsis-driver-fingerprint-ci.yml` provides an explicit **optimized** (default) or **debug** build choice under **Run workflow**. The generated artifact is named `synolsis-emu-adreno-v2-optimized-<run>` or `...-debug-<run>` for easier identification; both builds preserve the package ID. CI success confirms compilation, **not** game compatibility or an FPS increase.

## Controlled gameplay test

1. Export saves before switching signed APKs. The new package is different from the old Skyline/Strato installation, even if both Synolsis variants use the same package. CI debug signing may change if builder configuration changes.
2. Record selected driver *exactly* (Stable Turnip v2), date, game version/DLC, mode (handheld recommended for this phone) and resolution.
3. Keep the same **single spot of gameplay**, not just the animated main menu, for at least 2–5 min per test. Let shaders/cache warm up once.
4. Test optimized vs debug with every emulator option unchanged, and then test Fast Readback Writes ON vs OFF in the optimized build as an independent comparison.
5. Observe FPS plus average frametime and deviation, peak RSS, graphics correctness, crashes and thermal throttling. Record at least three tests in each configuration.
6. If optimized native code is unstable, go back to the debug baseline and attach the app log/short reproducible description. Do **not** clear saves or install unknown keys.

## Not implemented yet

True CPU/GPU decoded NVDEC acceleration, QLauncher system applet launch, generic internal 0.5× render resolution, FSR, async shader compiler, frame skipping, frame generation, shader backend alternatives and generic draw distance are separate native-core engineering tasks. Their Android toggles must only appear after a tested implementation exists. Never promise additional game-simulation FPS from generated/interpolated frames or simulated GPU time.

## Provenance and compatibility

Skyline/Strato core source, credits, and MPL/GPL obligations remain intact. Upstream Yuzu/Eden/Pine fixes require interface and license review before transplantation into the Strato renderer. No copyrighted games, keys, or firmware are distributed.
