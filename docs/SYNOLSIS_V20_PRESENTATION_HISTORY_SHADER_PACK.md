# Synolsis V20 — real presenter history tracking and packaged shader modules

Builds on successfully compiled V18 native MediaCodec/QLauncher experimental boot and V19 temporal shader prototype. Preserves the V5 graphics rollback baseline outside existing V15 final-presentation resize work.

## Implemented

1. `gpu/frame_history.h` and `tests/frame_history_test.cpp`: robust C++20 frame-history observation with monotonic timestamps, dimensions, formats and distinct image identity. Invalid/same-image/dropped timeline cases invalidate candidates. Estimate memory for three full RGBA8 images and reject dimensions exceeding a **48 MiB** soft budget (also cap each axis). No extra GPU images are held by this object.
2. **Real presenter integration:** `PresentationEngine::PresentFrame` observes pairs after the guest's presentation fence has completed and an image is known. `UpdateSurface` resets the history on Android surface recreation. A throttled debug message records candidate pairs, explicitly stating generated output remains disabled. Existing game FPS counters, Vulkan synchronization and frame timings do not change.
3. `scripts/build_synolsis_compute_assets.sh` builds/validates FSR1 EASU + RCAS shaders (AMD MIT license) and temporal interpolation GLSL into `app/src/main/assets/shaders/synolsis/*.spv`. The **V20 optimized APK CI** installs glslang/spirv-val, packages those real SPIR-V modules as Android assets and checks each asset exists inside the built APK. Builds outside CI must run this script before Gradle if they also want packaged assets.

## Not complete yet

- **Frame generation runtime output:** tracking candidate pairs is only metadata; there are no historical Vulkan *snapshots*, compute image descriptor sets, pipeline dispatch, synchronization or extra synthesized presentations yet. An observed pair is not an interpolated frame. No framegen option is exposed.
- **FSR rendering:** shaders are packaged, but the Vulkan command path still needs image allocation, EASU+RCAS descriptors, dispatch/barriers and output copy. A final-image bilinear downscale is **not internal resolution** or AMD FSR.
- **True internal resolution:** the existing V15 feature changes final swapchain image size only. Guest color/depth attachments and readbacks must be rescaled coherently before internal-resolution claims.
- **QLauncher:** user-provided system applet NCA bootstrap can be attempted, but Horizon services/home-menu navigation remain incomplete.
- **NVDEC:** Android NDK MediaCodec decoder backend exists and compiles, but guest host1x NVDEC method parsing, VIC texture conversion and guest synchronization remain unconnected.

No firmware, console keys or proprietary shaders are shipped. CI build completion validates compilation and packaging, **not** device playback, FPS improvement or enabled FSR/framegen.

CI: `.github/workflows/synolsis-v20-shader-history-ci.yml`. Keep release APK fallback when GitHub Actions artifact quota blocks normal upload.
