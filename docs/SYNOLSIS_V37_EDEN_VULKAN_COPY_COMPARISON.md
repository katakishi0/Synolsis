# Synolsis V37 — real Vulkan copy coherency / texture region repair

## Why this is a code repair rather than changing menu options

User tested NieR:Automata and Citron with rectangular magenta/blue/black corruption and flickering. A UI toggle for GLSL vs SPIR-V cannot correct an invalid Vulkan image copy or missing guest shader storage-image descriptors. Treat the Android Turnip driver, cached textures, shader translation and Strato's guest Maxwell interconnect as separate suspects.

Eden already has multiple **real** graphics fixes worth studying:

- Eden PR #321, commit `8ed05425dddf17aeafa4daa751cb3a3a13c5a31b`: identity component mappings for Vulkan storage-image views (reported to improve NieR graphics without fully fixing it). URL: https://github.com/eden-emulator/mirror/commit/8ed05425dddf17aeafa4daa751cb3a3a13c5a31b
- Eden PR #3511, commit `b5f9f8b7438a6a79644d712e267399d618f5d0e1`: image layout barriers, upload/present coherency and framebuffer resource lifecycle. URL: https://git.eden-emu.dev/eden-emu/eden/pulls/3511
- Eden PR #3853: image/view lifecycle, CPU/GPU cache flush and Vulkan synchronization improvements. URL: https://git.eden-emu.dev/eden-emu/eden/pulls/3853

**Important architectural difference:** Strato does *not* currently bind `Shader::ImageDescriptor` for `vk::DescriptorType::eStorageImage` in Maxwell3D or KeplerCompute. Both pipeline managers log **"Image descriptors are not supported"**, and the image usage builder does not declare `VK_IMAGE_USAGE_STORAGE_BIT`. Copying Eden's `StorageView` swizzle patch alone cannot work: the guest storage image pipeline and format support are absent. The feature needs its own scoped implementation with capability checks, descriptor binding, explicit identity-component views, storage layouts and per-stage synchronization.

## V37 fixes confirmed by inspecting Strato code

`Texture::CopyFrom` was already used for guest framebuffer -> Android swapchain copy; it contained three independent Vulkan correctness problems:

1. **Missing WAW/WAR memory ordering on already-transfer-destination images:** it skipped the destination image barrier entirely if the image had layout `TRANSFER_DST_OPTIMAL`. Vulkan layout equality does not imply synchronization. Now an image barrier is always issued before replacing contents, with correct no-access first use on `UNDEFINED`.
2. **Incorrect mip range end:** existing code compared `mipLevel < count` or `mipLevel < total-base` even when `baseMipLevel > 0`. A valid range `base=3,count=2` would copy zero levels. V37 validates ranges and iterates until `base + count` (or total for `VK_REMAINING_MIP_LEVELS`).
3. **Incorrect per-mip dimensions / array-layer confusion:** copy/blit always used full base texture width and height, and used `layerCount` for z offset. V37 computes each mip's width/height/depth and keeps Vulkan's independent `baseArrayLayer`/`layerCount`.

It also checks blit format feature support for **format conversion without scaling** as well as scaled blits.

Host C++20 tests `tests/vulkan_image_copy_region_policy_test.cpp` validate mip/range endpoints, layers, 3D depth and invalid ranges. Android CI also checks the new synchronization barrier and source wiring before NDK compiles.

## Not yet resolved

- No measured or guaranteed resolution of NieR/Nine Sols graphics issues, reduced FPS, Citron glitches or Turnip-specific behavior. The screenshot does not isolate a single offending shader/copy.
- Strato still needs guest storage-image descriptors and compatible image usage/layout; substantial missing Maxwell/Kepler compute shader functionality may cause incorrect results even with valid copies.
- Real render resolution 0.5/0.75/1.0 scaling needs guest render-target allocation and shader/transfer coordinate adjustment (issue #22), separate from presentation scale.
- Full guest NVDEC/VIC emulation and QLauncher Horizon services remain incomplete.

## Validation in gameplay

Compare V35 and V37 on the **same save**, portable mode, same driver version and graphics settings. Keep Frame Skip 0 and presentation scale 100%, Fast GPU readback hack OFF and GPU texture eviction OFF. Test NieR and a second title with multi-mip textures. Provide screenshots and emulation.log around visual corruption. Do not label the issue resolved until device rendering is visibly correct.
