# Synolsis V25 — Turnip flicker diagnostic and frame-skipping correctness

User-tested V20 and V22 with Turnip 26.3.0 r6 showed repeated black/red/yellow tiles, whole-frame corruption and around 10 FPS/100 ms. V22 compute pipelines do not include running FSR1 dispatch, so their mere availability does not repair corrupted guest textures.

## Changes versus V24 base

- **Vulkan source image transfer:** `Texture::CopyFrom` switches TOP_OF_PIPE -> TRANSFER barrier (incorrect for waiting on earlier shader/color/transfer writes) to ALL_COMMANDS -> TRANSFER with source MEMORY_WRITE and destination TRANSFER_READ. This is a conservative diagnostic fix for missing image availability dependencies, not proof of resolving every guest/Turnip bug.
- **Presentation:** conservative fixed 100% swapchain output in V25, disables the experimental 50%/75% *final* blit path and avoids per-frame Vulkan format-property probes. The internal game's color/depth target sizes are **unchanged**; FSR does not run.
- **Frame Skip:** new `PresentationSkipPolicy`, tested in `tests/presentation_skip_policy_test.cpp`, deterministically displays all frames in mode 0, every second in mode 1, every third in mode 2. Changes to the selected mode restart from one displayed frame. Guest fence and presentCallback are always processed even if host presentation is skipped, avoiding blocked/recycled graphics slots.
- **Frame history:** when a host frame is skipped, `frameHistory` is reset under `PresentationEngine::mutex`, so temporal generation candidates cannot span missing displayed frames and race with Android surface changes. This does not enable synthesized frames.
- Keeps V24 installed-firmware QLauncher scan + FileProvider, V13 AOC work, shader/SPIR-V assets and default app settings.

## Features not yet fully implemented

- Frame skipping saves some host-side presentation work, but does **not** skip emulated GPU draws, does **not** accelerate guest simulation and can reduce reported host display FPS. For the user's current low FPS and flicker, mode 0 is strongly recommended.
- Frame generation is **not running**: a temporal GLSL shader, planner, compiled compute pipeline and history metadata exist, but no pair of GPU image snapshots, fence-synchronized temporal dispatch or additional guest-safe swapchain presentation has been wired.
- FSR1 EASU/RCAS and true 50%/75% **guest internal** Vulkan rendering are unfinished. Keep these distinct from old final swapchain scaling.
- QLauncher firmware discovery does not guarantee missing Horizon menu services. NVDEC MediaCodec backend still needs guest host1x extraction and VIC/guest fence integration.

## Repro instructions

Use Turnip 26.3.0 r6 and same game/save/scene. For A/B, test V22 with the user's settings then V25 with shader cache *enabled* (turn OFF `Desativar cache de shaders`), `Liberar memória de texturas` temporarily OFF, FIFO/VSync, Frame Skip 0, scaled presentation 100%, and normal GPU frequency. Compare colored/black flicker, frametime and GPU memory. If V25 still corrupts, examine render-target aliases, layout transitions and shader translation; do not claim this fixed the issue.

Build from `.github/workflows/synolsis-v25-turnip-skip-ci.yml`, publish APK directly to GitHub Releases (storage Artifacts quota exceeded).
