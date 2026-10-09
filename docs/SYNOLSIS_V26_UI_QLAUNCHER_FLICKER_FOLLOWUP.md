# Synolsis V26 follow-up — QLauncher and Turnip flicker (2026-10-09)

## User reproduction
- Synolsis V25 test 3: the HOME/QLauncher problem persists.
- V25: gameplay still flickers. Previous V20/V22 reports mentioned black/yellow/magenta tiles with Turnip 26.3.0 R6.
- A new device screenshot or a short gameplay video is still needed to distinguish presentation tearing from source texture/target corruption.

## Changes staged on fix/synolsis-v26-qlauncher-compact-ui
1. Native firmware scan now opens header-only NCA first and checks program ID `0100000000001000` plus Program content type. Only matching candidates attempt full ExeFS decrypt and `rtld`, `main`, `main.npdm` validation. This completes the intended V26 optimization; it is not a Horizon menu-services implementation.
2. QLauncher tile uses installed firmware by default; manual NCA picker is optional via long-press, not forced after failure.
3. UI titles, descriptors, choices, and card spacing are compact in English and pt-BR. User-facing warnings remain accurate; V25 50%/75% final output scaling is explicitly inactive rather than falsely presented as internal resolution.

## Why V25 flicker is not solved yet
- V25 already emits a source-image Vulkan write-to-transfer-read image barrier in `Texture::CopyFrom` even when layout stays `TRANSFER_SRC_OPTIMAL`. Additional tweaks to the exact same barrier without evidence are unlikely to explain colored game textures.
- `PresentationEngine::PresentFrame` forces 100% output and does not dispatch the experimental FSR/temporal compute paths. Presentation-only scaling therefore cannot explain all remaining corruption.
- Open investigations: render-target and texture-cache alias/invalidation; guest surface format conversion; shader translation and precision; depth/MSAA resolve; host/guest fence lifetime and queue-image reuse. Determine first whether corruption is in source texture or only final swapchain.
- Do not enable FSR, temporal interpolation or forced fast GPU readback until a clean baseline exists.

## Relevant upstream technical references
- Khronos Vulkan synchronization: https://docs.vulkan.org/spec/latest/chapters/synchronization.html
- Eden Vulkan maintenance (MSAA resolve, attachment load/store, tiler behavior): https://git.eden-emu.dev/eden-emu/eden/pulls/4212
- Eden QLauncher regression fixes on Android and new firmware NS launch path: https://git.eden-emu.dev/eden-emu/eden/pulls/4407
- These are study references, not automatically compatible patches to Skyline/Strato.

## Testing matrix / acceptance
Use the same game, scene, save and driver version. Record a 3–5 s clip, 1 screenshot, whether the error appears in an in-app screen recording, GPU/driver name, firmware version, and FPS.
1. V25, Turnip 26.3.0 R6, frame skip Off, presentation 100%, FIFO, shader cache enabled, texture eviction off, safe/accurate readback.
2. Same config using the stock Adreno driver, if compatible, to isolate driver-specific behavior.
3. If flicker is still present, compare captured guest source texture with the displayed frame and instrument render-target aliases. Enable validation and GPU fault logs in development builds only.
4. QLauncher: installed firmware must be found without opening an NCA picker; if it exits, capture early native/Horizon service logs rather than claiming that finding the executable suffices.

**Status:** changes are source-only and not yet compiled or tested on Android. No flicker fix or full HOME menu runtime claimed.
