# Synolsis V34 — black-screen presentation diagnostics

## Ground truth from the V33 NieR:Automata emulation.log (2026-10-09)

The user reported an entirely black game image with the emulator overlay showing roughly 22 FPS, followed by an app closure on one launch. An inspection of the supplied log established:

- Title: `NieR:Automata The End of YoRHa Edition` (base program 0x0100B8E016F76000).
- Vulkan driver advertised `MesaTurnip 26.2.99`, Adreno 7c+ Gen 3, not the older driver version assumed by prior A/B advice. Driver builds should be compared by the **actual log**.
- A swapchain was created with `R8G8B8A8Unorm / Identity / Inherit / Fifo`. Successful swapchain creation does **not** prove any guest pixels were produced.
- The file contained no fatal Java exception, SIGSEGV tombstone, or Vulkan device lost indication. A separate Android logcat `crash` buffer is required to diagnose the app closure.
- Guest Maxwell commands were last recorded at approximately timestamp 1,832,092 (log tick); VI/queue buffers were last logged around 1,830,648.
- The `HOS-23` thread logged **168,766** identical `SleepThread: Waking any appropriate parked threads and yielding` records. These began at log timestamp 1,918,754 and continued through 4,062,851, obscuring other diagnostics and stressing CPU/log output.
- Approximately 219 audio renderer manager services were created by `HOS-16`, which could be a downstream retry loop.
- `MergeStaging: Pipeline cache append failed; restoring previous length` appeared once; a potentially damaged cache is worth A/B clearing, but causality with black output is **unconfirmed**.
- The independent Android MediaCodec decoder is not connected to guest NVDEC registers/VIC; the title loads `NintendoSDK_movie`, but video decoding failure is **one possibility**, not proof of the black frame.

## V34 code changes

1. Rate-limit the hot `SleepThread(-1)` log: first four messages and powers of two only. **Do not** change the Horizon scheduler's timing, logic or wake-up behavior.
2. Sample guest `PresentFrame` submissions and guest fence completions (first four / every 120th frame) to distinguish a rendering stall from a present failure.
3. Log source image dimensions/layout and `vkQueuePresentKHR` result to identify presentation errors.
4. On `VK_ERROR_OUT_OF_DATE_KHR`, mark the swapchain stale for recreation on the next guest frame while still honoring the guest buffer release callback. Other present failures now report the error rather than being silently ignored.

## Reading the new logs

- No `Synolsis present begin`: guest VI/BufferQueue is not handing off frames (or presentation is being skipped before the renderer).
- `present begin` but no `fence ready`: blocked waiting for the guest GPU/host1x fence.
- `fence ready` but no `present result`: the texture copy/synchronization/acquire path needs investigation.
- `present result=Success` but black image: guest texture contents, video decode, and GPU format/aliasing remain suspects; success means accepted by Vulkan WSI, **not** that correct pixels exist.
- `OUT_OF_DATE` / unexpected Vulkan result: inspect surface lifecycle, swapchain recreation and Android window behavior.
- App closure with no emulator fatal log: request Android `logcat -b crash -d -v threadtime` or tombstone; do not guess a crash reason.

## Suggested next A/B test

Use identical NieR scene, installed base content, mode **handheld**, present mode FIFO, Frame Skip 0, presentation 100%, and the exact same installed Turnip driver in V33 and V34. Compare visible image, guest present status, FPS and RAM. Try disabling custom Vulkan driver or changing driver as a *separate* test. Do not present NVDEC, Nine Sols/NieR rendering correctness, internal resolution scaling or QLauncher HOME Menu as completed.

This is a **diagnostic build**, not a verified graphical fix.
