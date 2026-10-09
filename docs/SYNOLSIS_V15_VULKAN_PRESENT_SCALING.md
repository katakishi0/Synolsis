# Synolsis V15 — Vulkan host presentation scaling

## Implemented (native path + Android settings)

- Added per-title/global `vulkanPresentationScalePercent` (100%, 75%, 50%, default 100%) via `EmulationSettings`, `NativeSettings`, `AndroidSettings` and `common/settings.h`.
- `PresentationEngine::PresentFrame` now chooses an output swapchain extent based on the user's selected percentage. It checks host Vulkan blit support, surface min/max dimensions, applies swapchain recreation only when the target changes, rescales Android crop coordinates and calls `Texture::CopyFrom(..., permitScaledBlit=true)` when necessary.
- `Texture::CopyFrom` uses Vulkan `vkCmdBlitImage` to resample when source and destination image dimensions differ; it checks format feature flags, requires one mip for scaling and falls back to nearest filter when linear sampling is unsupported. Ordinary 100%/same-format calls still take the original `vkCmdCopyImage` path.
- Pure geometry/crop code and test in `gpu/presentation_scale.h` and `tests/vulkan_presentation_scale_test.cpp`; English and pt-BR UI labels explicitly state the limitation. This is a **working final-output resize path** intended for GPU bandwidth and window compositor experiments, *not* the actual internal render resolution of emulated games. The game still produces full-sized render targets, so substantial FPS gains are **not guaranteed**.

## Not implemented or activated

- **True Vulkan internal resolution:** must rescale emulated color/depth render targets, framebuffer aliases, guest->host copies, viewport/scissor/readbacks and resolve operations together. The older geometric helper is not sufficient to enable this without content distortion.
- **FSR 1 EASU/RCAS:** bilinear Vulkan blits are not AMD FSR. Production FSR requires shader passes, descriptors, intermediate images, barriers, color-space and device feature checks. See upstream AMD FidelityFX FSR1 MIT license before bundling source.
- **QLauncher:** actual OS application/menu applet with process/service handoff still missing.
- **NVDEC/MediaCodec:** current `soc/host1x/classes/nvdec.cpp` doesn't decode video; a decoder needs NVDEC method/register extraction, memory mapping and MediaCodec output synchronization.
- **Frame generation:** no temporal interpolation or synthesized frames. Displayed FPS must be tracked separately from game simulation FPS.

Keep V13 AOC/BKTR implementation and V5 GPU rollback untouched beyond the isolated scaled-presentation code path. No firmware or console keys are distributed. Full Android ARM64 APK build and on-device test required before recommending 50/75% as stable.
