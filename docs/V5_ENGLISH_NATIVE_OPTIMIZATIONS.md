# Synolsis Emu V5 — English default and native Adreno hot-path improvements

This branch extends the **V4 Android ARM64 Strato core** without changing Android package `org.synolsis.emu` or the native `org.stratoemu.strato` JNI class namespace.

## Implemented in code

1. **English on first installation** (`SynolsisLocaleBootstrap.kt`): if there is no app-specific language selection, set `en` exactly once per install on first Application creation. Existing app-specific language selections, including `pt-BR`, are not overwritten. A dedicated stored initialization marker prevents overriding the user when reopening the app. The **App Language** picker in **Settings → Theme and Color** continues to offer English, Português (Brasil), other supported languages, and **Use System Default**.
2. **Vulkan presentation hot-path** (`gpu/presentation_engine.cpp`): remove the unused `NATIVE_WINDOW_GET_NEXT_FRAME_ID` ANativeWindow perform call executed for every presented frame. The returned frame ID was never read and did not influence the guest, Vulkan synchronization, or the VI callbacks. This avoids one native-window IPC per presented frame.
3. **Shader/pipeline cache worker I/O** (`gpu/pipeline_cache_manager.cpp`): commit staging cache header after **32** cache bundles instead of 16 during sustained shader compilation; still commits when the queue becomes idle. This reduces header seek/flush traffic under heavy burst compilation. If force-killed during a sustained burst, as many as 31 trailing records may be ignored; they can be recompiled next launch.
4. **Background worker contention:** release `writeMutex` before waking the shader cache writer with `notify_one()`. Correctness preserved by the queue's condition-variable predicate.

CI workflow: `.github/workflows/synolsis-v5-english-perf-ci.yml`. Validates the locale bootstrap, localization picker, shader cache threshold, absence of unused frame ID query and Android/Kotlin + ARM64 native APK compilation.

## Limitations and benchmarks

- UI string resources in `values/` provide English fallback; where translators already supplied localized strings, switching locale still shows those translations.
- **System Language** in the emulated Switch settings is separate from **App Language**; changing the menu to English must never rewrite HOS title language or region preferences.
- The three CPU/GPU hot-path adjustments aim to reduce unnecessary CPU work and flash I/O, **not guarantee higher average FPS** in NieR:Automata. Measure the same game version and scene, same Turnip driver, 5-minute run, and warm shader caches across V4/V5. Record average/1% low FPS, frametime variation, peak RAM and graphics anomalies. Keep low-memory 6GB Adreno defaults safe.
- The first-run locale marker lives in a dedicated preference file; clearing app data resets it. A user may explicitly select **Use System Default** after initialization and the selected mode is preserved on subsequent launches.

## Next stages (not claimed complete)

| Feature | Technical prerequisite |
| --- | --- |
| **0.5×/0.75× resolution and FSR** | Scaled Vulkan guest render targets, viewport/scissor coordinate translation and safe framebuffer/compositor upscaling |
| **NVDEC hardware / fallback** | Verify Tegra NVDEC/VIC HLE APIs then implement Android MediaCodec decoder with suitable image format, synchronization and fallback |
| **QLauncher (HOME Menu)** | Boot system applet `0100000000001000` with real authorized firmware and system services (NS/AM/VI/FS/HID), correct process and applet transitions |
| **Frame generation** | GPU interpolator, frame history and presentation scheduler integration, on-device benchmark of latency/VRAM cost; does not accelerate emulated game logic |
| **Updates and DLC mounted** | Verify CNMT metadata and patch/base title linkage, implement NCA BKTR patch overlays, DLC content registration through FS/AOC services; V4 NSP storage alone is **not activation** |
| **Shader/backend improvements** | Verify changes from Yuzu/Eden in a distinct branch; integrate compiler IR/Vulkan paths separately and keep provenance/license headers |

See existing GitHub issues #9 (QLauncher), #10 (graphics) and #15 (native content support). Keep each milestone in an isolated branch and require real compiled builds and device tests before exposing incomplete settings.
