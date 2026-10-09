# Synolsis V36 — GPU debug flood / runtime stability experiment

## Evidence (V35 user capture)

The provided `emulation.log` was ~151 MiB (158,281,342 bytes) across **1,647,223** lines, covering ~92 s of emulator activity.

- The V35 SVC 0x18 repair is exercised: empty-handle waits reached powers-of-two sampled counts; **no** `SetTerminateResult: 60929`, `Exit Stack Trace` or `Break` in this capture.
- `Synolsis present begin/fence ready/source` all appear. Exactly **2,252** `Synolsis present result` records showed `SuboptimalKHR`. This is Vulkan WSI acceptance, **not proof of correct source pixel contents** or a fatal Vulkan error.
- `GPFIFO` thread output alone recorded 316,029 `CallMethod` lines, 109,009 `operator()` lines, 38,501 `WriteSemaphoreResult` and 29,269 `CompleteDma` events. Logging at this rate can reduce CPU headroom in debug mode and consume storage; FPS impact must be measured.
- Shader compiler reported 20 `(STUBBED) called` warnings and 5 `Geometry shader passthrough used with no support` warnings. These are compatibility investigation targets; the compiler contains a geometry passthrough **lowering** implementation already, so warnings alone do not establish a rendering failure.
- The audio renderer repeatedly created services (8,409 `IAudioRendererManager` requests). This remains under investigation for service lifetime/performance; no behavioral change in V36.
- The capture reported `MesaTurnip 26.2.99` and `Adreno 7c+ Gen 3`; keep the same driver across comparisons.

## V36 change

- The two hottest GPFIFO debug sites now log only the first eight calls and calls whose sequence index is a power of two.
- The pushbuffer processing and batch-end DEBUG messages use the same sampler.
- `VK_SUBOPTIMAL_KHR` remains an accepted result: report first four/every 120th frames only, while **always** reporting unexpected Vulkan failures.
- In non-debug sessions, hot methods skip the atomic/logging work via the existing logger-level check.
- Tests compile the sampling policy and verify the source of the V34 Vulkan fence and SVC checks is still present.

This **does not modify** guest commands, scheduler, Vulkan barriers, texture aliasing, shader translation, NVDEC, or swapchain configuration. It cannot by itself guarantee a flicker fix or faster release-mode performance.

## User acceptance

A/B on the same phone and saved scene: V35 vs V36, same Turnip and portable mode, frame skip 0, standard display scale 100%. Measure FPS and frame time **with debugging enabled** to observe logging overhead; separately compare without debugging to determine gameplay baseline. Keep graphical defect screenshots distinct from performance.

## Planned GPU follow-up

If visible corruption or black image persists with successful `present result`, investigate the guest framebuffer content/render target aliasing (Maxwell/Vulkan), scissor V29 vs V30 behavioral difference as a separate A/B variant, and actual shader compiler warning sites. Do **not** equate `VK_SUBOPTIMAL_KHR` to a fatal crash. NVDEC guest decoding/VIC/Horizon services and QLauncher remain incomplete.
