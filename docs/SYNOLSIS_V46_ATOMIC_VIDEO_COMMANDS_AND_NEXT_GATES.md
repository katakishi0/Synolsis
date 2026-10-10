# Synolsis V46 — Host1x video command continuity and atomic validation

This is a **code/test checkpoint, not an APK release or proof that cutscenes work**.

## Proven source defects beyond V45
1. `VideoCommandProbe::Inspect()` reset Host1x target class to class 1 for every separate nvhost gather. Real guest command streams may send SetClass once and send THI method writes in later buffers on the same channel. V45 silently discarded those writes. V46 persists current class per video channel across gathers.
2. V45 dispatched early THI method pairs before reaching an unsupported or truncated opcode later in the *same gather*. That could partially invoke NVDEC decoding and then reject the rest of the buffer. V46 performs a bounded dry-run on a copy of class state and only forwards on fully validated syntax. No spoofed decoded-frame success, no guest syncpoint changes and no copying GPL code.
3. The selected class and THI method state are only committed after successful validation. Tested with NVDEC and VIC sample pushbuffers.

## Scope limits and remaining blockers
- This does not implement full Host1x Gather/Restart/relocation support or accurate asynchronous syncpoint semantics. Invalid/unsupported video packets are skipped by the containing nvhost compatibility path.
- The V44 Android MediaCodec and VIC YUV420 output are **conditional**. Real cutscenes still require end-to-end guest bitstream validation, supported output strides/crop/tiling, and GPU fences/guest surface visibility on physical Android hardware.
- QLauncher remains incomplete: frontend URI errors and Horizon HOME services are different failure stages. No firmware or console keys are bundled.
- True internal resolution must allocate render targets independently of guest texture size and coherently handle texture-cache aliases, viewport/scissor, sampling, DMA and CPU readback. A presenter-only downscale is not enough and stays disabled.
- Frame generation must produce a GPU-owned interpolated image from synchronized prior/current textures with a separate presentation timeline. Packaged SPIR-V files and metadata plans do not satisfy this. Do not enable a fake UI switch.
- Preserve V39 Maxwell/Kepler Vulkan storage-image correctness and V35 Horizon SVC startup fix.

## CI acceptance and next development
- Compile `tests/synolsis_v44_video_command_probe_test.cpp` for legacy V45 behavior.
- Compile `tests/synolsis_v46_atomic_video_gather_test.cpp` for cross-gather continuity and fail-closed malformed buffers.
- Keep V44 SMMU/H264 and VIC YUV conversion regressions.
- Build the full Android Kotlin + native ARM64 source **without distributing an APK**.
- Only after real Android GPU/cutscene testing and completed resolution/framegen integration can these features be described as functional.

Baseline repo branch: `feature/synolsis-v45-guest-video-dispatch`. V46 branch: `feature/synolsis-v46-video-dispatch-atomic-gathers`.
