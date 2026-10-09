# Synolsis V30 — compatibility and stability checkpoint

**Source base:** V28 Synolsis, not the V29 GPU rollback. Portable/handheld is the default for **new installs only**; existing per-title/global preferences remain as chosen.

## Actual code changes

- **Maxwell3D/Vulkan:** signed 64-bit region arithmetic with clamped scissor/clear rectangles; prevents signed underflow and invalid overly large rectangles when an attachment is smaller than the guest crop. No promise of a fix for all shader, texture aliasing or driver issues.
- **SVC:** handles zero-length WaitSynchronization and invalid handles as guest errors rather than host crashes; avoids underflow in GetInfo available-memory query. Other missing SVCs remain unimplemented.
- **NVDEC diagnostic path:** rate-limits unimplemented-register logging (prevents flooding at video playback). **Host MediaCodec backend:** bounds timestamps and decoded dimensions and stops a failed session instead of leaking a dequeued codec input slot.
- **NVHOST submission:** validates gather offsets/byte ranges and syncpoint IDs before signalling any guest fence; unsupported relocations return a guest error, not a host exception.
- **Portable default:** existing saved docked settings are intentionally not overwritten.

## Explicitly NOT implemented

- Full NVDEC: the host1x/NVDEC register parser, SMMU address translation, codec bitstream extraction, hardware decode completion, VIC YUV->RGBA copying and guest fences remain to be integrated together. Adding MediaCodec by itself does **not** play in-game videos.
- Full HOS SVC coverage, original QLauncher HOME system services, 0.5x/0.75x **internal** resolution, FSR GPU dispatch or frame generation.
- A verified fix for *NieR:Automata*, *Nine Sols* or any specific title. Needs device logs/repro.

## Controlled testing (same saved scene, same driver)

1. **V29** original Strato GPU and handheld setting — earlier control.
2. **V28** Synolsis with shader/presentation adjustments — earlier control.
3. **V30** only added bounded-scissor, SVC and NVDEC/video safety fixes on top of V28.
4. Use Turnip 26.3.0 R6 consistently. Keep handheld mode, frame skip 0, Vulkan present FIFO/Auto, cache on, no max clocks. Change one option at a time.
5. Observe black/magenta rectangles, flickering, image-within-image, missing textures, video playback and sound, FPS/frametime, RAM and crashes for at least 3 minutes in the same scene. Collect logcat/renderer logs before reconfiguring.
6. If V30 worsens rendering, compare with V28 and V29, do not merge graphics changes blindly.

## Next work

- Investigate image layout/feedback loops, depth/color alias invalidation, texture dirty tracking, Vulkan render-pass barriers and shader translation bugs; use Vulkan validation layers and an instrumented renderer if reproducible.
- Implement a complete NVDEC guest register -> bitstream -> Android decode -> VIC surface -> host1x fence pipeline with format-specific tests (H264/VP9 first).
- Audit additional Horizon SVCs from actual game traces; do not stub success for unsupported calls.
- Measure long-frame percentile and shader pipeline cache misses under a 6 GB RAM budget, with a separate accuracy vs performance profile.
- Internal resolution 0.5x/0.75x integration is tracked in GitHub issue #22.
