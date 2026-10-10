# Synolsis V49 — NVDEC H.264 access unit framing (experimental)

## Why this change matters

V44–V48 built a real partial chain from guest Host1x NVDEC registers through bounded SMMU reads, MediaCodec, VIC YUV420 and packed RGB output. However, the NVDEC bridge handed *raw guest bitstream bytes* straight to Android MediaCodec. A length-prefixed H.264 NAL stream is not Annex-B. MediaCodec's byte-stream decoder may reject it, even with correctly generated SPS/PPS.

## Source changes

- `nvdec_h264_access_unit.h`: independent, MPL-licensed H.264 access unit normalizer.
- Accepts complete Annex-B with three/four-byte start codes, or complete 4-byte big-endian length-prefixed NALs converted to four-byte Annex-B start codes.
- Validates maximum input size (8 MiB), maximum 4096 NALs, header forbidden bit, non-zero recognized NAL type, entire NAL lengths, presence of an actual picture NAL and full packet boundaries before passing data onward.
- Rejects truncated, unknown or oversized formats **before** invoking MediaCodec; does not fabricate pictures and does not invent codec SPS/PPS.
- `nvdec.cpp` feeds validated normalized bytes to existing H264 CSD and MediaCodec, retaining the existing guest SMMU checks, codec constraints, VIC bridge, guest command dispatch and known-working V39 Vulkan correction.
- Host unit tests cover Annex-B, length-prefix to byte-stream conversion, malformed and oversized buffers and fail-closed cases. GitHub Actions runs these along with all prior V44–V48 tests and builds Android Kotlin/ARM64 **without publishing an APK**.

## Not yet a functioning cutscene claim

Decoder packet framing is one step only. Switch/NVDEC guest streams may have other H.264 framing, custom slice ordering or SPS/PPS requirements; MediaCodec output may have unsupported color plane/stride/crop layouts; VIC target surfaces, guest visibility/fences and asynchronous syncpoint semantics still need Android in-game validation. There is no verified end-to-end cutscene capture in this version.

The project still lacks **real guest internal resolution** and **real Vulkan frame generation**. Presenter-only scaling is deliberately disabled on Turnip; exposing a GUI knob does not implement render target resizing or synthetic frames. QLauncher/Horizon HOME remains unverified and incomplete.

## Checkpoints and safeguards

- V35 — fixed empty WaitSynchronization boot failure.
- V39 — user-confirmed Vulkan Maxwell/Kepler storage-image graphics correction; preserve.
- V43 — latest user-testable APK at the time of V49 work.
- V47 and V48 — successful independent source/ARM64 CI, **no published APK**.
- V49 — test-only source increment; **not an APK release**.

## Next engineering gates

1. Collect redacted `emulation.log` from the specific cutscene to identify whether the guest H264 stream format and VIC output are actually supported.
2. Validate decoded output plane layout/crop, real guest SMMU writes and GPU visibility; implement proper asynchronous NVDEC/VIC syncpoints once guest semantics are established.
3. Create an internal-resolution design tying guest framebuffer dimensions to physical render target dimensions in texture cache, rasterizer viewport/scissors, DMA aliasing and CPU readback. Implement native-only golden-image regression tests before enabling a UI setting.
4. Implement frame-generation only when synchronized previous/current *GPU images*, compute descriptors/barriers and a separate presentation timeline are functioning in Vulkan. No fake FPS multiplier and no forced FPS unlock.
