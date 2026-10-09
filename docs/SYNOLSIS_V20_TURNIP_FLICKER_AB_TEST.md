# Synolsis V20 — Turnip 26.3.0 R6 flicker regression A/B build

User supplied on-device screenshots from the V20 Android APK with approximately 10 FPS / 96–101 ms per frame and ~2.9 GiB reported memory. Visible symptoms: alternating very large solid black/magenta/yellow regions, disappearing rendered scene fragments and a full black view with controller overlay intact.

## Controlled changes relative to V20
- `gpu/texture/texture.cpp`: presentation source barrier was **TOP_OF_PIPE + srcAccess MEMORY_WRITE**, which is not a valid availability dependency for earlier GPU writes. Changed to **ALL_COMMANDS -> TRANSFER** with the same access masks, so shader, color-attachment and copy writes are available to transfer reads. The rest of the texture upload/format pipeline is unchanged.
- `gpu/presentation_engine.cpp`: deliberately forces final presentation scale to **100%** regardless of the UI preference for this diagnostic APK. Removed three Vulkan physical-device format queries formerly executed each displayed frame. The Android crop and swapchain logic stays at native dimensions.
- Retains all other V20 game/driver settings and the V5 native GPU baseline. Keeps SPIR-V EASU/RCAS/temporal assets *inactive* as before.

These two changes are grouped into **one temporary conservative build** for first symptom isolation; do not claim causal attribution or FPS improvement until real A/B device testing. The experimental 50%/75% present-scale option is intentionally ineffective in this build only.

## Safe diagnostic settings on the phone

1. In Vulkan GPU settings, toggle **Desativar cache de shaders** OFF — so shader caching is enabled.
2. Set Vulkan presentation resolution 100%, FIFO/VSync, skip 0, leave Disable Presentation Throttle OFF.
3. Temporarily turn **Liberar memória de texturas do jogo** OFF (avoid repeated evictions / texture recreation). Keep fast GPU reads/writes OFF and do not enable subgroup-disable.
4. Test force-triple-buffer OFF first, then ON in a separate run, as it can raise latency/VRAM demand and different drivers cap swapchain images.
5. Leave Force Max GPU Frequency OFF initially to avoid thermal escalation while comparing; if performance remains low, run 3-minute tests with temperatures and battery charge level.
6. For strict A/B, do not alter the driver simultaneously: test V20 then the safe build using the same Turnip 26.3.0 R6, save scene, settings and phone temperature.

## What this does NOT fix

If whole-frame corruption persists at native presentation with valid barriers, likely culprits move to guest GPU shader translation, render targets, texture alias invalidation, render pass layout, or driver-specific Turnip 26.3.0 R6 behavior. Gather the first Vulkan validation-layer error if feasible and compare a different Turnip build separately. Frame generation, real FSR upscale and real internal resolution are NOT enabled by this build.

Release only after GitHub Actions compiles ARM64 with CCache and publishes `app-mainline-optimized.apk` via GitHub Releases; Actions artifact storage quota is exhausted. Preserve the V20 release intact.
