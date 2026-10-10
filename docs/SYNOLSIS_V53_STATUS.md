# Synolsis V53 — incremental MediaCodec reliability

Base: V52 commit `451c3c4aa80939ff6159b6c1ed17b597eab27fd8`.

## Implemented
- Handle `AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED` with a nonblocking follow-up output dequeue before VIC gives up.
- Keep V52 decoded frame limits, crop checks, queue bounds, Vulkan fixes and QLauncher behavior unchanged.
- Add a focused source regression check for this transition.

## Not yet implemented or validated
- Real game cutscenes still require testing of NVDEC bitstream parsing, Android color layouts, VIC guest-memory surface output and synchronization. This change alone does not make cutscenes work.
- Resolution control currently affects host presentation size, **not** the guest's internal render targets. True internal scaling requires coordinated render-target, texture aliasing, viewport/scissor and copy behavior; simply changing swapchain extent is not enough.
- Existing frame-history/compute experiments are **not** proof of functioning frame generation; guest frame pacing and duplicate/interpolated frame presentation must be validated.
- QLauncher HOME requires more Horizon services and runtime diagnosis; loading an NSO is not sufficient.
- No device validation was performed by this source-only change. Do not label an APK stable or feature-complete on the basis of a CI pass.
