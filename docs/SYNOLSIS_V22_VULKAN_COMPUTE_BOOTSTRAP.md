# Synolsis V22 — Vulkan compute pipeline bootstrap

Source branch: `feature/synolsis-v22-vulkan-compute-pipeline-bootstrap`. Based on V21 bounded asset manifest and completed V20 ARM64 APK.

## Native integration in this branch

- `gpu/packed_compute_manifest.h` defines the three exact APK asset paths (FSR1 EASU/RCAS + experimental temporal interpolation) and bounded SPIR-V header validation, with portable C++20 unit tests.
- `gpu/packed_compute_pipelines.h` loads real binary SPIR-V from Android `assetFileSystem`, enforces a 1 MiB per-file cap and valid alignment, then creates real `vk::raii::ShaderModule`, `vk::raii::DescriptorSetLayout`, `vk::raii::PipelineLayout` and `vk::raii::Pipeline` compute resources through the live GPU Vulkan device. FSR bindings are uniform buffer, sampled image, storage image, sampler. Temporal interpolation bindings are three combined samplers plus one storage image, with its 16-byte push constants. Construction is atomic through RAII and does not allocate intermediate frame images.
- `PresentationEngine::UpdateSurface` bootstraps the pipeline set once per Vulkan device after the surface is validated, logs success and safely falls back to original native presentation if any asset/driver initialization fails. It never rebuilds a compute pipeline each frame.
- `.github/workflows/synolsis-v22-vulkan-compute-ci.yml` validates the unit tests, shader compilation and packaging, native ARM64 Vulkan build and publishes an APK through GitHub Releases if Actions artifact storage is unavailable.

## Exact limitations

This is **real Vulkan pipeline creation** but still **not operational FSR upscaling or frame generation**. Pipelines have no live `vkCmdBindDescriptorSets`, image descriptors, `vkCmdDispatch`, barriers, output textures, or frame timing/snapshot synchronization. We intentionally keep the established V5/V15 native presentation path intact and no fake settings toggles are exposed.

True **internal resolution** is independent from the existing final-presentation swapchain scaling. Implementing it requires guest color/depth surface alias resolution, host image allocation at different sizes, coordinate and copy/readback transformations and correctness tests.

The user-provided QLauncher applet bootstrap still lacks complete Horizon home-menu services. The V17 MediaCodec backend still needs host1x NVDEC bitstream extraction, VIC surface conversion, and guest GPU fence integration. Actual Switch update BKTR RomFS activation still requires correct base patch pairing and IVFC/ExeFS mapping. Frame-gen shader remains a prototype until real temporal GPU image history and presentation are connected.

No firmware, keys, proprietary blobs or extra runtime FPS claims are included.
