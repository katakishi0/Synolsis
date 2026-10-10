# V56 — V54 device logs, persistent C++ build cache and shader cache repair

## Evidence received
Two user-supplied logs, 2026-10-10:
- `emulation.log (1)`, ~18 KB, ~305 lines.
- `emulation.log (2)`, ~14 MB, ~150,854 lines.
Both refer to NieR:Automata and Mesa Turnip on Adreno. Avoid sharing full device logs in public repositories.

**Direct video blocker: VP9, not H.264.**
Both logs record `NVDEC hardware codec selected: id=0x9`, `NVDEC execute count=N codec=0x9`, `NVDEC guest frame unavailable: reason=codec-not-yet-supported`, and `VIC guest image not available: reason=nvdec-output-not-ready`. The native protocol maps `0x9` to VP9. V54/V55 only reconstruct H.264 NVDEC picture/bitstreams, so AVC MediaCodec work does not fix this title. V56 changes the diagnostic to `vp9-guest-picture-header-reconstruction-not-implemented` (no fake VP9 support).

**VP9 work needed before cutscene claims:** independently implement/validate Tegra NVDEC VP9 picture info, reference frames and entropy probabilities, generate the correct VP9 compressed and uncompressed headers, feed a valid VP9 access unit to `video/x-vnd.on2.vp9`, then validate VIC YUV plane/crop/tiled output and guest video syncpoints. License review required before using any GPL code in this MPL project. Codec selection Auto/Hardware/Software/Hybrid works only for formats actually parsed by the guest decoder.

**Vulkan shader cache:** one warning in log 2: `MergeStaging: Pipeline cache append failed; restoring previous length`. The game nevertheless loads 28 graphics pipelines from existing cache in 9 ms. `PipelineCacheManager::MergeStaging` attempted to stream-insert an empty `.staging` file whose header count is zero; this can set `std::ostream::failbit` and warn despite zero meaningful data. V56 skips/removes only empty staging while retaining the main cache, and validates nonempty staging payloads before append. Other causes of append failure remain possible and require additional file-state evidence.

**Build cache:** previous workflow set `CCACHE_DIR` and Gradle `--build-cache` but did **not** persist the C++ `.ccache` directory across GitHub hosted runners. V56 restores/saves it via `actions/cache@v4` with branch-specific cross-run keys, plus Gradle setup-gradle cache and `ccache --show-stats`. First run of a new branch is cold; subsequent commits should show compiler hits. Cache accelerates builds, not in-game FPS.

**Secondary unrelated observation:** log 2 warns `Optional Vulkan compute module unavailable: ... shaders/synolsis/easu.spv`; native presentation continues. This is separate from the VP9 blocker and must not be sold as working Vulkan FSR.

**QLauncher:** neither emulation log contains `Synolsis-QLauncher` Android launcher errors. FileProvider/ContentProvider failures happen before emulation and may appear only in Android Logcat. Do not diagnose the latest QLauncher problem solely from these two game logs.

## Verification checklist
1. GitHub CI: C++ host checks; Kotlin and full native ARM64 APK; inspect real cache hit/miss counters on **second** V56 build.
2. Vulkan shader cache: with the same game and driver, confirm 28+ prior cached pipelines continue loading and zero-count staging does not produce spurious append warning. Do not clear caches for this regression test.
3. NVDEC: VP9-specific warning must replace misleading generic code; actual VP9 cutscene remains pending.
4. QLauncher: collect private `logcat` lines tagged `Synolsis-QLauncher` (redact identifiers/paths if shared) on firmware-home launch.
5. Preserve V35 boot, V39 storage images, saves and title-specific GPU drivers.
