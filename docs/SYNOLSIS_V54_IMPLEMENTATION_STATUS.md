# Synolsis Emu V54 (experimental user-feedback checkpoint)

Base: V53 aligned-output (fa3e24085d4a3e506fadc76e615a3abb8abfa672); do not merge `main` blindly.
Reference: V35 WaitSynchronization boot and V39 Vulkan STORAGE_IMAGE correctness.

## Changes backed by code
- Per-game AppDialog Update and DLC buttons replaced with **Complementos/Add-ons**.
- GameContentManagerActivity shows both staged update and DLC lists, retains integrity verification/deletion, and has an anchored Install button with Update/DLC choice.
- UI short cache action label: **Limpar cache/Clear cache** (actual cache delete logic unchanged).
- Per-game GPU driver preference surfaced immediately below **Use custom settings** (already backed by per-TitleID driver selection, not a new renderer).
- Vulkan Lite one-shot profile uses existing native-backed options: handheld, 3 executor slots, 256 flush threshold, 2 shader workers, triple buffering OFF, throttling ON, shader cache ON, no experimental readback/direct import/forced clocks, no presentation skip, stable present mode; restart game.
- Existing low-end presets no longer enable aggressive guest texture reclamation. New installations default guest texture reclamation OFF. Old saved preferences are not reset forcibly.
- QLauncher FileProvider failure now has bounded private-cache staging fallback for a validated NCA and reports failure when caching is impossible.
- INFO logs now identify actual MediaCodec output rejects and VIC guest surface writes without leaking firmware/keys/guest pixel data.

## Limitations — do not claim these are fixed
- NVDEC cutscenes **NOT PROVEN** visible on an Android device; frame-to-renderer sync/coherence and H264/VIC layout may still need changes after logs.
- QLauncher HOME **NOT PROVEN**; only FileProvider staging fix. Horizon services/app lifecycle remain incomplete.
- CNMT/BKTR/AOC update/DLC **staging != mounting**. This V54 UI does NOT activate purchased game add-ons yet.
- Vulkan Lite can increase stutter or lower FPS on some drivers; tested source change != performance benchmark.
- Actual guest internal resolution and Vulkan FSR/Frame Generation are not implemented.
- Experimental graphics readback options may cause rendering errors, keep OFF during baseline.
- Backup saves before testing experimental APK.

## Required test matrix
1. Install V54, open NieR card; verify 'Limpar cache', one 'Complementos' entry, Update and DLC sections and Install selector; import only user-owned NSPs.
2. Open game-specific settings -> Use custom settings ON -> GPU driver immediately visible -> choose Turnip and confirm on launch.
3. Compare baseline vs Vulkan Lite using same Turnip driver and 3-5 minute scene, log FPS/frametime, note stalls/crashes and RAM.
4. QLauncher launch: report `Synolsis-QLauncher` lines to separate provider/cache success from loader/Horizon failures.
5. NVDEC: report `NVDEC guest frame unavailable`, `NVDEC decoded Android frame`, `Synolsis NVDEC MediaCodec output rejected`, `Synolsis NVDEC/VIC guest frame write`, `VIC guest image not available`, and Vulkan texture/update warnings; redact paths/tokens.
6. Regression: NieR V39-good shader visuals, basic boot V35, save data, controller UI, global settings, no regression to persistent graphics options.
