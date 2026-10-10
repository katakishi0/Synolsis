# Synolsis V46 — guest video stability (developer checkpoint)

## Source baseline
Forked from V45 guest NVDEC/VIC dispatch, retaining V39 Maxwell3D/KeplerCompute storage-image descriptor compatibility, V35 Horizon SVC 0-handle WaitSynchronization, and V43 visible UI corrections.

## Concrete changes
1. `video_command_probe.h`: **two-phase packet validation** — parse a copy of the register-selector state and counters without emitting any NVDEC/VIC calls. Only fully valid Host1x gathers are then replayed to the actual sinks. A malformed gather cannot execute its valid prefix or poison the persistent method selector.
2. `android_media_decoder.cpp`: allow a finite 4ms wait for MediaCodec input slots, rather than dropping the frame whenever `dequeueInputBuffer(0)` initially reports Try Again Later.
3. `android_media_decoder.cpp`: output drain supports a caller-specified wait bounded to 12ms; handles the immediate format-change event by querying the next buffer; use actual frame dimensions when Android omits optional stride/slice-height properties.
4. `nvdec.cpp`, `vic.cpp`: VIC requests a decoded frame with up to 12ms wait (NVDEC polling remains nonblocking), without fabricating decoded frame data.
5. Host regression: `tests/synolsis_v46_atomic_video_dispatch_test.cpp`. Verify malformed prefix emits *zero* downstream NVDEC/VIC method calls and register state is unchanged.

## Unfinished / do not claim as working
- This remains a **candidate**, not demonstrated playable cutscenes. Real guest H264/VIC output must be verified on an ARM64 device, including correct guest SMMU addresses, codec data, frame formats, tiled output, and syncpoint ordering.
- Native VIC path only covers a subset of YUV420. Other pixel formats, hardware block layouts, H265/VP8/VP9, multiple surfaces, reordering and dynamic frame size need explicit support.
- Host frame time and guest frame generation are different concerns. The current `gpu/presentation_engine.cpp` observes metadata history but does not retain synchronized *Vulkan image snapshots*. It cannot dispatch temporal interpolation or show frame-generated FPS yet.
- GPU **internal resolution** must scale guest render targets, viewport/scissor, depth and texture semantics. The existing `vulkanPresentationScalePercent` path is final presentation scaling and is forced to 100% because Turnip previously flickered. It must not be mislabeled as internal resolution.
- A successful Android APK compiler run is not a visual cutscene demonstration. Do not publish a user test release as *complete* until representative games and ARM64 drivers are validated.

## Verification
- Host-only test: `g++ -std=c++20 -O2 -Wall -Wextra -Werror -pedantic -Iapp/src/main/cpp tests/synolsis_v46_atomic_video_dispatch_test.cpp -o /tmp/v46video && /tmp/v46video`.
- Full Android Kotlin + C++ ARM64 CI: `.github/workflows/synolsis-v46-native-video-validation.yml`, **no release**.
- Capture on actual device: `NVDEC execute`, `NVDEC guest frame unavailable`, `VIC guest image not available`, and Vulkan errors, while showing a known cutscene. Do not share console keys.

## References
- Citron Host1x NVDEC/codec and VIC architecture (reference for concepts and offsets only, not copied from GPL-licensed project): https://github.com/citron-neo/emulator/tree/main/src/video_core/host1x
- Citron H264 codec component incorporates MIT-licensed Ryujinx-origin work; future adaptations require license review and independent tests.
