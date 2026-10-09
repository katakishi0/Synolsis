# Synolsis V31 — Android Vulkan WSI compatibility experiment

This build carries forward the V30 Maxwell scissor/SVC/NVDEC stability changes, handheld default and earlier Synolsis features.

## Changes tested

- Vulkan swapchain creation now picks only a **supported** surface transform and alpha compositing mode. Older Strato hardcoded `INHERIT` alpha and implicitly used `IDENTITY` transform, which can violate Android surface capabilities (VUID 01279/01280).
- Vulkan swapchain image format is checked against **advertised** surface formats every time, not guessed. The RGBA/BGRA fallback only runs when that format really exists on the surface.
- A small pure C++ unit test exercises the capability-choice policy. These checks are about correctness; they do not prove that title-specific shader/texture bugs are fixed.

## Playtest matrix

Test the **same title, save, driver, game profile and scene** on:
1. V29 rollback — original Strato GPU control.
2. V30 — saturating Maxwell scissor and safe SVC/NVDEC handling.
3. V31 — additional Vulkan Android WSI validity fixes.

Keep handheld mode, Frame Skip 0, shader cache ON, presentation at 100%, FIFO/Auto present mode, fast GPU readback hacks OFF. Change only one setting per comparison. On a 6 GB phone, test at least 2 minutes in gameplay and report FPS, stutters, flicker, black/pink textures and RAM. Record a short video of *Nine Sols* if possible. Logs should contain a line starting `Swapchain WSI:` showing the format/transform/alpha/present mode.

## Important limitations

This is **not** a proven flicker fix for *Nine Sols* or *NieR:Automata*. Unsupported shader translations, texture aliasing, ASTC decoding and Turnip issues may still produce corruption. Android MediaCodec exists only as an **independent backend**; guest NVDEC register decoding, bitstream extraction, VIC output and fence completion are not wired. Most missing Horizon SVCs and QLauncher services remain unfinished. A real 50/75% internal render-scale path is tracked in issue #22.

## Next steps

- Diagnose graphical differences with comparable video frames and logcat to identify Maxwell texture alias, sampler, shader translation or WSI cause.
- Implement guest host1x/NVDEC/H264 command parsing and VIC output behind a verified fence pipeline, without fake successful decode.
- Audit SVC semantics from failing title logs; reject invalid inputs with Horizon-compatible errors and unit tests.
- Merge only the changes that pass both CI and real device playtests.
