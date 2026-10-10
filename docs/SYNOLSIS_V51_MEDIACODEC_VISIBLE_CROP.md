# Synolsis V51 — MediaCodec crop rectangle support, developer checkpoint

## Source changes beyond V50

- V49 immutable Host1x guest command snapshots (max 1 MiB) and request-for-NV12 decoder output are kept.
- V50 integrates guest H264 Annex-B / 4-byte length-prefixed NAL normalization and existing VIC packed RGB + NV12 code.
- V51 reads Android MediaCodec's reported crop-left/top/right/bottom when all four coordinates are present, ordered and inside its output dimensions. It uses inclusive right/bottom to compute **visible** output width and height.
- `AndroidMediaDecoder::Frame` now retains visible width/height and crop origin while preserving the codec's padded row stride and slice height.
- VIC `PackLinearNv12` and `PackLinearRgba` can copy an **even-aligned, bounds-checked crop** from planar or semi-planar YUV420 buffers without inventing pixel strides or moving the guest output window.
- Odd origins, output too large for the backing Y plane, unknown/opaque Android color formats, and incorrect destination surface size are rejected. Failed staging does not mutate previously staged guest bytes.
- Host C++20 regressions `tests/vic_mediacodec_crop_test.cpp`, `tests/vic_rgb_output_test.cpp`, `tests/vic_yuv420_output_test.cpp` and the existing video tests run in the new GitHub Actions workflow. Full Android ARM64 compilation is requested; no APK is automatically published.

## Why it matters

Video decoders may advertise a padded hardware picture and an independently cropped visible area (for example a coded height 1088 while the visible video is 1080). The previous V48/V50 VIC path required exact uncropped dimensions and could refuse the output despite valid video pixels. This is a genuine source-code improvement but is not proof a game cutscene is now visible.

## Known limitations that remain

- Android `COLOR_FormatYUV420Flexible` with unknown plane/pixel stride is still not interpreted. Guessing a buffer layout would corrupt a guest texture.
- No guarantee of synchronization with guest nvhost syncpoints; the current compatibility acknowledgement is not the same as asynchronous video completion.
- Frame order, color range/matrix and VIC scaling/composition remain partial. GPU cache visibility after SMMU writes requires physical tests.
- **True internal resolution and Vulkan frame generation are unimplemented**, because render-target allocations, texture aliasing, per-pass scaled viewport/scissor, and retained synchronized image snapshots/compute/presentation are still disconnected.
- QLauncher HOME system services remain incomplete. No console keys are embedded or required to compile.

## Acceptance

Before user APK release: full native CI + emulator launch test; run the same cutscene on physical Android and inspect NVDEC real codec info, MediaCodec reported color+crop, VIC writes, and screen output. Confirm the V39 graphics patch still renders correctly with the same Turnip setup.

## V51 Android NDK build repair

- Full ARM64 CI failed because Android NDK 26.1.10909125 does not export `AMEDIAFORMAT_KEY_CROP_LEFT/TOP/RIGHT/BOTTOM` macros in `NdkMediaFormat.h`.
- Native `AMediaFormat_getInt32` accepts a string key. Replaced the nonexistent macros with Android MediaFormat's stable keys `crop-left`, `crop-top`, `crop-right`, `crop-bottom`, retaining the four-field/bounds-checked fallback.
- Updated V51 host regression to check these actual key strings. A full Android native CI rerun is still required to verify the repair; not evidence of working in-game cutscenes.
