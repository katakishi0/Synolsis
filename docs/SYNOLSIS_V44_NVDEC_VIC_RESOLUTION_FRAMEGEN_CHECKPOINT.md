# Synolsis V44 — native video bridge checkpoint

## State: experimental, not a released or verified game feature

This branch continues the **V43 base** (V35 SVC boot fix, V39 Vulkan Maxwell/Kepler storage-image graphics repair).

### Implemented for NVDEC → Android MediaCodec

- Native Host1x NVDEC method bank with 0x80 codec, 0xC0 execute, 0x101 picture info, 0x102 bitstream; guest register addresses are hardware-shifted by eight bits.
- Strict 32-bit SMMU region bounds (including wraparound) and explicit mapping validation *before* reading guest packet/context buffers.
- Reads actual H.264 picture info and compressed bytes from the running game; never fabricates a video frame or reports success on an empty buffer.
- Inspects real stream length, progressive macroblock dimensions, 8-bit 4:2:0 mode and format before passing a packet to Android's native MediaCodec.
- Produces conservative SPS/PPS from guest metadata when a compressed packet contains no parameter sets; validates H264 NALs. This does not support every H264 profile, scaling list, bitstream layout or interlaced field.
- A single bounded Android decoded-frame handoff to VIC is retained (no unbounded queue).

### Implemented for VIC linear output

- Connects VIC to the *same channel's* NVDEC via THI without copying arbitrary guest addresses.
- Handles guest VIC config/luma/chroma offset methods and execute trigger.
- Only supports linear YUV420 guest output, with Android planar or semi-planar 8-bit YUV420 CPU buffers and known row stride/slice height.
- Builds padded, 256-byte-aligned luma and interleaved UV buffers with allocation caps and checks guest SMMU destinations before writing.
- Refuses block-linear formats, RGB output, ambiguous Android flexible YUV420 layouts, mismatched sizes and unmapped/overflowed guest allocations.
- Tests cover C++ protocol selection, incomplete NCA/legacy GPU baseline, Annex-B header requirements, SPS/PPS generation, NV12 conversion, padding, invalid address/memory guards.

**Important:** This is a conditional partial path, **NOT proof that any cutscene plays**. Hardware decoder output formats and game surface layout still require device testing; block-linear swizzling, crop/transfer and guest syncpoint correctness must be implemented.

### QLauncher

The user's V43 error was the **generic Android exception handler** in the frontend, not necessarily a native NSO/Horizon failure. V44 now distinguishes:
1. failure to create a FileProvider URI;
2. failure to read that URI;
3. failure to launch EmulationActivity.

Logcat uses the prefix `Synolsis-QLauncher`. This diagnostic change is **not** a HOME implementation; Horizon services, applets and applications remain incomplete.

### Resolution and frame generation — still blocked by architecture

**Real internal resolution** is not the host swapchain size. To implement 0.5×/0.75×/2× correctly requires:
- separate guest dimensions vs Vulkan render target dimensions in texture/view cache;
- scaled scissors/viewport and clear rectangles in Maxwell 3D;
- shader sampling, depth/stencil and mipmap consistency;
- guest→host texture copies, DMA, aliasing and CPU readback synchronization;
- framebuffer presentation/crop and cache invalidation;
- per-game fallback if resolution scaling would corrupt the game.

**Frame generation** currently has metadata/planners and packaged shaders, but **no guest image history, Vulkan descriptor/image resources, compute dispatch + barriers, or twice-per-frame presentation scheduler**. Duplicating a frame or adding only a setting is not frame generation. Real interpolation increases latency and will need timing guards and disabled-by-default game profiles.

Neither feature should be promoted as working in UI or published release notes until compiled and tested on a real device.

### References studied (architectural facts, not GPL source copied)

- Citron H.264 NVDEC code (Ryujinx MIT headers): https://github.com/citron-neo/emulator/tree/main/src/video_core/host1x/codecs
- Citron Host1x register paths: https://github.com/citron-neo/emulator/tree/main/src/video_core/host1x
- Citron resolution scaling (GPL code used only as architectural reference): https://github.com/citron-neo/emulator/blob/main/src/common/settings.h
- The Synolsis V39 patch remains unchanged in Maxwell/Kepler pipeline managers.

### Acceptance gates

1. Host C++20 tests pass.
2. GitHub Actions Android Kotlin and ARM64 C++ compile pass with no fatal errors.
3. Play a test cutscene and capture `NVDEC` / `VIC` logs to confirm whether decoder output is linear or block-linear (no console keys should be shared).
4. Confirm NieR gameplay still renders without the V20–V38 corruption.
5. Only after live cutscenes, genuine internal render scaling and image-interpolation output are confirmed should this become a released feature build.
