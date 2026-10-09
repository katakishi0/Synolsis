# Synolsis V38 — optional accurate guest GPU fence callbacks

## Evidence and source comparison

The user reports that Citron and Synolsis show large black/magenta/blue rectangular corruption on the same game, while Eden may have resolved some flicker conditions. Eden merged **PR #4004** on 2026-05-24:
https://git.eden-emu.dev/eden-emu/eden/pulls/4004

The merged Eden commit `eabd1017ccec18eadfa3f05971d09a1fb4c77097` changes
`src/video_core/fence_manager.h` to allow an opt-in `antiflicker` setting
to make GPU completion callbacks wait on GPU work even when ordinary async
flush tracking doesn't require waiting, with reported lower performance
impact than using universally high GPU accuracy.

Strato differs architecturally: `gpu/interconnect/command_executor.cpp` was
observed to execute `pendingDeferredActions` and the user callback
**immediately after** `SubmitInternal()` whenever
`useDirectMemoryImport == false`. This is the common Turnip path.
When `useDirectMemoryImport == true`, the same callbacks were already
queued behind a `FenceCycle` in `ExecutionWaiterThread`.

These callbacks include *guest GPU syncpoint increments* and *semaphore
releases*, which can unblock guest work while preceding host GPU commands
are still running. The Vulkan fence itself can be functioning perfectly,
but the guest is incorrectly notified before the related GPU writes finish.

## Implementation

- Android settings -> GPU: **Anti-flicker (GPU fences)**, stored per game
  and available globally, English and pt-BR labels.
- Default **OFF**, because waiting for GPU completion may reduce FPS and we
  must not declare the compatibility gain without device testing.
- Native setting `gpuFenceAntiFlicker` is passed from
  `EmulationSettings` -> `NativeSettings` ->
  `AndroidSettings` -> `CommandExecutor`.
- When enabled, the non-DMI path queues guest callbacks through the
  existing `ExecutionWaiterThread` after the corresponding
  `FenceCycle`. It does not reimplement Yuzu/Eden fences, add global
  `vkDeviceWaitIdle` or change guest commands.
- When disabled, the existing fast path is unchanged. DMI path retains its
  prior accurate callback ordering regardless of the toggle.
- Unit test `tests/gpu_fence_callback_policy_test.cpp` validates all four
  DMI/antiflicker combinations; CI validates wiring through all files.

## Device acceptance criteria

1. Keep the V35/V36/V37 APK and backup your saves.
2. Test V38 with the same game, driver, save, mode portable,
   presentation 100%, Frame Skip 0.
3. Compare with Anti-flicker **OFF** then **ON**, fully restarting the game
   after each setting change. Compare black/magenta artifacts, flicker,
   FPS, frametime and RAM.
4. If enabling it fixes flicker but hurts performance, optimize which guest
   signals truly need waiting, not by disabling Vulkan correctness.
5. Recheck Vulkan present source image and actual guest shader/storage
   images for any residual corruption.

## Distinguish from other incomplete features

- No proof yet that this option fixes NieR, Nine Sols or other titles.
- Strato still lacks proper Maxwell/Kepler `eStorageImage` descriptor
  binding and image `VK_IMAGE_USAGE_STORAGE_BIT` resource usage.
  Eden's separate NieR storage-image identity swizzle repair cannot simply
  be copied without that missing infrastructure. It is a separate
  substantial compatibility project.
- GLSL/GLASM code exists in shader-compiler, but **Android Vulkan uses the
  SPIR-V compiler/pipeline**. Showing an unconnected backend selector would
  be misleading; a real shader-backend option needs compiled and compatible
  GPU backends with format and cache versioning.
- NVDEC/VIC, QLauncher and true internal render scaling remain incomplete.

This is an opt-in *behavioral* anti-flicker patch and a CI-tested
implementation. It must not be marked as graphically fixed before real
device A/B confirmation.
