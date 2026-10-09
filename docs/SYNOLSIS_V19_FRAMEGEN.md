# Synolsis V19 — experimental temporal interpolation module

Built upon V18 QLauncher user-firmware NCA bootstrap, V17 NDK MediaCodec and V16 AMD EASU/RCAS reference shaders.

## New native code

- `gpu/frame_interpolation_planner.h` computes the midpoint of two *distinct* guest-presented frames, accounting for the cadence. Invalid timelines, too-fast/too-long gaps and incomplete GPU image history are rejected; no fake increase to the emulator's game FPS.
- `gpu/shaders/framegen/temporal_interpolate.comp` compiles to Vulkan SPIR-V and synthesizes one actual RGBA intermediate frame from two image textures, optionally warping sample coordinates using an upstream-provided normalized motion field. Without a motion field the shader performs temporal color interpolation and may ghost on fast motion; **it is not AMD FSR 3/3.1 or motion-compensated generation yet**.
- `tests/frame_interpolation_planner_test.cpp` covers valid 60/30 fps deltas, invalid/out-of-order timestamps, valid history and GPU fences. GPU shader compiled/validated via `glslangValidator` and `spirv-val` in GitHub Actions.

## Still needs runtime integration

Actual presentation of synthetic frames requires:
- buffer and lifetime management for **both** previous and current completed Vulkan images;
- GPU semaphore/fence and image-layout transitions, descriptor pool and storage target;
- Vulkan compute pipeline creation and queue dispatch, followed by image-to-swapchain copy and accurate timestamp pacing;
- explicit generated FPS vs guest simulation FPS counters, extra frame latency warnings;
- a real motion vector source or optical flow to avoid excessive ghosting;
- 6 GB RAM budget and fallback on insufficient device features.

This module is valuable/real computation, but **not yet a working framegen option**. Do not add an inert toggle to settings.

V15 presentation scaling is separate from internal render-target scaling. AMD FSR1's MIT shader binaries exist in V16, but Vulkan EASU/RCAS runtime passes still need connection. QLauncher requires user-owned firmware and more Horizon applet services. NVDEC needs register parsing/VIC and guest fence synchronization before MediaCodec decoded video appears.
