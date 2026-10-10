# Synolsis V45: guest Host1x video command path (experimental)

## Root cause confirmed in source

The nvhost `Host1xChannel::Submit` implementation validated guest command buffer spans but commented out `Push(gather)` and advanced *all* guest syncpoint counters immediately. The native NVDEC→MediaCodec→VIC code introduced on V44 could therefore receive **no actual nvhost video methods** from this submit route, irrespective of working backend unit tests. This explains why simply adding a MediaCodec wrapper or NVDEC/VIC logging in V42/V43 did not fix cutscenes.

## Change made on V45

- New independent MPL-2.0 Host1x pushbuffer method decoder with bounds checks: SetClass, Incr, NonIncr, Mask and Imm. Invalid/unsupported opcodes fail closed, not followed into arbitrary guest addresses.
- Synchronously inspect buffers **before advancing the nvhost syncpoints**, while nvmap memory handles are still alive.
- For NvDec (0xF0) and VIC (0x5D) forward **only THI Method0 (0x10) and Method1 (0x11)**, which supply guest registers to the V44 decoders. No raw channel method 0 (syncpoint increment), no extra host1x/general acceleration.
- NVDEC/VIC share the V44 per-channel decoder instance. Existing SMMU limits and H264/VIC output validation remain in force.
- Lock video dispatch per channel to keep Method0/Method1 pairs and frames ordered and prevent simultaneous decodes on one decoder instance.
- If a guest input is malformed, do not blindly call unbounded host1x gather recursion. Logs only sample early and power-of-two video submits.
- The nvhost guest syncpoints still use **legacy count-based acknowledgement**; this is not accurate async NVDEC fence management. The feature is experimental. A decoded MediaCodec frame and YUV surface output are only conditionally supported depending on guest H264/stride/color/tiling.

## Tests and what they prove

- A standalone host C++20 decoder test checks actual THI sequences, codec/execute detection, truncation, unhandled packet rejection and method forwarding. The test uses synthetic **command words**, not fake video data.
- Existing V44 tests assert SMMU guest bitstream guards, SPS/PPS generation and VIC packing bounds.
- Android Kotlin/ARM64 build verifies ABI and native compile when GitHub Actions completes.
- **No physical Android game/cutscene test** exists yet, and this branch is deliberately not configured to publish an APK.

## Outstanding work (do not claim complete)

1. End-to-end test cutscene output on Android, inspect codec ID, H264 picture context, SMMU mapping, MediaCodec output format and VIC guest destination. Add all necessary color/crop/plane layouts and synchronization.
2. Guest syncpoint/fence semantics and commands beyond five basic Host1x opcodes. Generic gather execution and relocation remain unsupported; old async FIFO retains pointers to memory with unsafe lifetimes and should not be re-enabled blindly.
3. True **guest render resolution** (0.5×/0.75×/2×). The present-only scaler is **not** internal resolution; framebuffer/texture cache, viewport/scissor, DMA, sampling and CPU-visible readback must be upgraded as a group.
4. True **frame generation** requires owned prior/current GPU image snapshots, Vulkan compute image barriers and descriptor binding, output surface interpolation and frame-pacing with an independent render scheduler. Frame history metadata and shader assets alone do not generate frames.
5. QLauncher may fail before native execution (Android FileProvider) or inside Horizon services; its V43 generic frontend error is not evidence of one specific native loader failure. Capture `Synolsis-QLauncher` logcat to determine stage, then fix the underlying error rather than swallowing exceptions.

**Testing policy:** do not label V45 as a working or complete emulator release. Keep V39 as a graphics regression baseline and V43 as the last published APK checkpoint until end-to-end device verification is available.
