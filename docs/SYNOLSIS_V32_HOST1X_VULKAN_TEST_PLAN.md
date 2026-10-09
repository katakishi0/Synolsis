# Synolsis V32 — Host1x hardening and Vulkan suboptimal-image fix

Base: V31 Vulkan swapchain format/alpha/transform capability checks, plus V30 Maxwell scissor, SVC/NVDEC safety and handheld default.

## Native changes

- **Vulkan presentation:** Treat `VK_SUBOPTIMAL_KHR` returned by `vkAcquireNextImageKHR` as an **acquired image** with a signaled semaphore. The upstream Strato loop retried immediately with the same already-signaled binary semaphore, violating Vulkan synchronization and risking stalls/corruption.
- **Swapchain replacement:** Wait for Vulkan device idle before replacing an existing swapchain and clear retired frame fences. This is not a per-frame wait; it happens only on recreation.
- **NVHOST:** Validate the syncpoint ID returned for a video channel and return a guest error when no usable syncpoint exists.
- **Host1x:** Validate that each command-gather word exists *before* advancing the iterator; the original `*++entry` could read past a truncated buffer. Also bound-check fixed-size syncpoint arrays in Host1x and THI.
- **VIC:** Rate-limit unsupported-register logging; frequent per-method logcat writes are avoidable work on constrained Android devices.

## Not implemented yet

These safety fixes do **NOT** decode Nintendo Switch videos. The NVDEC guest registers, bitstream extraction, SMMU, H264/VP9 access-unit pipeline, VIC YUV conversion and accurate GPU/CPU fences are not connected. Unsupported Horizon SVCs and HOME Menu compatibility remain separate tasks. No real internal 0.5x/0.75x rendering or frame generation is claimed.

## A/B test on-device

1. Preserve a backup of saves and record Turnip driver version. Use handheld mode, shader cache ON, Frame Skip 0, presentation scale 100%, FIFO/Auto.
2. Compare exactly the same saved *NieR:Automata* or *Nine Sols* scene: V29 (original GPU), V30 (Maxwell/SVC), V31 (Vulkan surface), V32 (acquire/recreation).
3. Observe black/magenta blocks, duplicated small frames, stutter, crashes, RAM and guest FPS for at least 3 minutes. Record screenshots and logcat when possible.
4. If V32 worsens any behavior, revert to V31; do not infer FPS gains from build-only tests.

## Acceptance criteria

The native source compiles with NDK, CI verifies the guarded paths, APK is published as a test prerelease, and the user confirms no regressions in the same scene. True fixes to title-specific graphics require device-level validation.
