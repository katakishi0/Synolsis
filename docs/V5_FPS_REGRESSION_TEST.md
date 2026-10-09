# Synolsis V5 — FPS regression isolation (user report 8 FPS default, 15–18 FPS after tuning)

## Report and baseline
- User installed the APK from GitHub Actions run **37860507764**, job **113594831232** (artifact `synolsis-emu-v5-english-native-perf-optimized-1`).
- Same user reports 8 FPS at default settings, and at most 15–18 FPS after changing settings; a previous Synolsis version reached around 24–25 FPS in the user's tests.
- **No matching scene/driver/firmware/thermal logs were supplied yet. This is a measured user-reported regression, not a proven root cause.**

## Exact source comparison
The above V5 job log confirms a checkout of commit `6983f22343d565dc071e72e970b7213f6e2efeb0` at build time, regardless of the workflow commit on `main`. The V4 baseline is `fdbed7086fcef53201c4586fada61ea6bb2c3084`.

Compared to V4, tested V5 changed only two native files:
1. `gpu/presentation_engine.cpp` removed `NATIVE_WINDOW_GET_NEXT_FRAME_ID` from the per-frame present path.
2. `gpu/pipeline_cache_manager.cpp` increased cache header batching from 16 to 32, and moved the writer `notify_one()` outside the queue mutex.
Frontend changed to use a fresh-install English locale bootstrap and is not expected to explain a persistent in-game FPS regression.

## Controlled rollback
The test branch `fix/synolsis-v5-fps-regression-rollback` branches from **exactly V5 6983f223** and restores **the full contents** of the two native files above from V4. No other CPU/GPU setting, Java locale, app package ID, compiler flag, shader backend, Vulkan driver manager, or graphics cache directory is changed.

This is **a diagnostic**, not a claim that the FPS will recover.

## A/B protocol (on Snapdragon 778G / Adreno 642L)
1. Export/back up save files and record firmware, Turnip ZIP/driver version, game update, game settings, graphics preset, readback and shader cache state. CI APKs may be signed by different debug keys; an Android update may need uninstall/reinstall, which erases app data. Prefer export first.
2. Compare V5 APK against diagnostic rollback build in **the same game and same menu/scene**, matching all settings. Do not compare an unmodified default with a manually tuned build. Use the **same graphics driver** and emulated Switch resolution.
3. Give each build a warm-up period; measure at least 60 s of steady-state FPS and frametime variation, repeat tests as A/B/A if possible, and note temperatures/throttling. Shader cache warms up at different rates after reinstall.
4. If rollback recovers the expected FPS, bisect the two native files with individual single-change builds to determine the real cause. If both builds regress, compare a **verified V4 artifact** with identical settings, inspect driver and kernel/Turnip changes, and collect logcat/Perfetto frametimes.
5. Do not merge into main or enable new experimental defaults until user device results verify the improvement.

Build action: `.github/workflows/synolsis-v5-fps-regression-ci.yml` (CI static assertions + Kotlin/native compilation + APK artifact).
