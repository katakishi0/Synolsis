# Synolsis V27 — isolated Turnip synchronization test, QLauncher and compact UI

This branch follows the user-tested V25 APK, whose colored/black flicker and QLauncher failures persisted. Do **not** claim the graphics or HOME menu regression is resolved until tested on hardware.

## Vulkan changes

- `Texture::CopyIntoStagingBuffer`: replace the invalid BOTTOM_OF_PIPE -> TRANSFER producer dependency with ALL_COMMANDS (MEMORY_WRITE) -> TRANSFER (TRANSFER_READ). The former cannot wait for prior color, compute, shader or transfer writes.
- `Texture::CopyFromStagingBuffer`: initialize previously undefined images using TOP_OF_PIPE with zero source access, instead of HOST/MEMORY_READ.
- `Texture::TransitionLayout`: use ALL_COMMANDS dependencies and nonzero source/destination access to order real image reuse before following consumers, with no source access for discarded/undefined old layouts.
- `Texture::CopyFrom`: include prior writes in the destination image dependency before the transfer copy.
- Keep the conservative V25 100% output, Frame Skip=0 as test baseline and avoid enabling experimental FSR/frame generation. These barriers may add overhead; measure performance and correctness on Turnip.

## QLauncher changes

- The V26 `NCA(..., headerOnly=true)` constructor is now used during *installed firmware* scanning. Only matching Program-ID NCA candidates undergo full ExeFS decryption.
- Existing `rtld`, `main`, `main.npdm` verification, secure FileProvider and user-owned firmware directory are retained.
- If automatic scan finds nothing, explain that keys/firmware need checking instead of unexpectedly launching SAF. Long-press the QLauncher card for the explicit manual NCA fallback.
- This does **not** implement missing Horizon system services or prove that QLauncher boots (not even with firmware 22/23).

## UI

- Smaller Material cards, typography and margins, shorter section descriptions, English and Brazilian Portuguese.
- Frame Skip/VSync/shader thread captions are localized to pt-BR.
- Remove nonfunctional 50%/75% *output* scale choices. True guest internal resolution is still unimplemented.
- Condense staged update/DLC text while retaining that staged content is not applied.

## Reproduction protocol

1. Install the resulting APK as the same variant as V25 (preserve/export data first); import keys and firmware supplied by the user.
2. Use the same Turnip revision (previously 26.3.0 r6), the same game/save/camera and the same shader-cache profile.
3. For the first A/B run use FIFO, Frame Skip 0, output 100%, shader cache ON and temporary texture freeing OFF, both in V25 and V27.
4. Compare colored flicker, missing tiles, total time and in-game FPS/frametime after equal warmup and for an equal-duration session.
5. If it still flickers, capture short video, timestamp, GPU driver and adb logcat; proceed to render-target aliasing/texture view lifetime, access masks, cross-queue fences and shader translation. Avoid describing pipeline initialization as FSR dispatch.

## CI acceptance

- Build Android/Kotlin/JNI/ARM64 app in GitHub Actions.
- Run existing native geometry/CNMT/frame-skip tests.
- Assert header-only candidate filter and safe staging-memory dependencies.
- On success, publish APK via GitHub Releases; running a build is **not** proof of a runtime flicker fix.
