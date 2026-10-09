# Synolsis V31 — Vulkan surface, Horizon SVC and host1x safety

## Base
V30 core compatibility branch, including the existing Maxwell scissor fixes, installed-firmware QLauncher diagnostics, portable default **for new installs**, guarded SVC waits and experimental MediaCodec host backend.

## Implemented in V31
- **Vulkan:** `gpu/surface_policy.h` selects only preTransforms and composite-alpha modes actually advertised in `vkGetPhysicalDeviceSurfaceCapabilitiesKHR`. Prefers identity transform (the Android native-window path already applies transformation) and inherit alpha, with supported fallbacks. Throws an explicit error if none are advertised.
- **Host1x FIFO:** validates the number of remaining method argument words before incrementing the guest gather pointer, with a reusable helper and executable C++20 tests. Prevents out-of-range reads on malformed method packets.
- **Host1x syncpoints:** rejects IDs outside the 192 available Tegra X1 syncpoints in both THI and host1x class method paths, avoiding an unchecked `std::array::at` failure.
- **Horizon SVC:** adds `svcSignalEvent` (0x11) for valid `KEvent` handles; `svcClearEvent` now validates the handle type and preserves `InvalidState` when nothing could be reset.
- **Native C++ tests:** exercise Vulkan capability masks and host1x FIFO length checks.

## Unfinished — do NOT advertise as functional
- *Nine Sols*, *NieR:Automata*, flickering / colored blocks: the root cause is still unverified. It may involve Maxwell image/texture aliasing, missing shader conversions, render passes or Turnip driver behavior. Swapchain capabilities can only fix an invalid swapchain mode, not all rendering defects.
- **NVDEC complete playback:** host1x gathers are not passed into decoding, NVDEC register methods do not reconstruct codec bitstreams, guest SMMU addresses are not translated, VIC conversion and decoded texture/fence writeback are not connected.
- **SVC coverage:** only SVC 0x11 was added in V31. Numerous newer HOS calls remain unimplemented.
- **Internal resolution scaling** 0.5x/0.75x, GPU FSR dispatch, genuine frame generation and working QLauncher HOME Menu.

## A/B test
1. Use legally dumped *Nine Sols* or *NieR* and the same game update, save/state, scene, Vulkan GPU driver and temperature window for each run.
2. Compare V29 (original GPU), V30 and V31 for at least 3 minutes each, with handheld mode, shader cache enabled, frame skip 0, no readback hack, max GPU clocks off.
3. Record which of: black blocks, magenta/yellow patches, duplicate inset image, missing cutscene/video, audio stutter, startup failures, memory usage and average/1%-low frametime occurs.
4. Preserve logcat and native Strato/Synolsis logs, especially `Validation Error`, `VkResult`, `NVDEC`, `svc`, `host1x`; do not post encryption keys.
5. Treat measured FPS improvement only when the scene and driver are comparable. Never claim game-specific compatibility from a successful build.
