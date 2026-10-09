# Synolsis Emu V6 — incremental roadmap and status

Branch: `feature/synolsis-v6-content-integrity-render-foundation`, based on the **compiled V5 FPS rollback** (`fix/synolsis-v5-fps-regression-rollback`). Do not merge experimental renderer work over stable baselines without device comparison.

## Delivered in this step

- **NSP SHA-256 at import**: stream the digest in the same 128 KiB copy loop (no extra whole-file buffering) and record it in the sidecar JSON.
- **Avoid same-title duplicate packages**: compare size and recorded digest within the chosen game/update-or-DLC category, rejecting a duplicate before finalizing the staged file. Legacy packages without a digest remain safely listed.
- **Verify imported NSP**: visible per-entry **Verify** button asynchronously rehashes an imported NSP and reports OK or corrupted; English and pt-BR translation included. A matching hash proves only that the private copy still equals the imported file: it does **not** verify Nintendo signatures, CNMT, title ID or mount the content in the game.
- **Native render-target geometry foundation**: `gpu/render_scale_geometry.h` provides overflow-safe 0.5× / 0.75× / 1× output extents. `tests/render_scale_geometry_test.cpp` compiles with host C++20 and tests fixed cases and odd resolutions. This code does **not** yet change Vulkan guest allocations. The only live rendering scale remains **1×**; no misleading settings/UI option was added.

## Missing native integrations, prioritized

| Milestone | Requirement | Status |
| --- | --- | --- |
| Vulkan internal resolution + FSR | Retarget guest color/depth allocations, framebuffer aliases, texture copy/resolve, viewport/scissor, normalized crops, readbacks, and per-game settings. Add Vulkan upscaler with capability checks and a fallback. | Render-geometry tests added; renderer **not integrated** |
| Real NSP updates | Cryptographically parse CNMT, validate base/patch title compatibility, overlay BKTR and ROMFS/ExeFS correctly using original encrypted NSP backing. | NSP import/integrity works; activation **not implemented** |
| Real DLC | Parse CNMT/AOC content IDs, mount NCA data and update `aocsrv` list/count and FS service. Never report items as mounted without live backing. | **Not implemented** |
| QLauncher | Obtain the user's own compatible system applet content; implement OS process/app mode transitions and missing system services. | **Not implemented** |
| NVDEC | Map Tegra decode HLE requests to Android MediaCodec with proper decode buffers, fences, timestamps and VIC image formats; CPU fallback. | **Not implemented** |
| Frame generation | Real temporal frame interpolation and history, latency/memory accounting, fallback when frames are invalid, and separate displayed FPS from game FPS. | **Not implemented** |
| 6 GB Adreno tuning | Profile Vulkan memory budget, shader compilation queues and CPU/GPU sync; compare under the same driver/scene with device recordings. | Existing V3/V4 controls preserved; further work **pending** |

## CI and safety

`.github/workflows/synolsis-v6-content-render-ci.yml` compiles the native geometry test and a new Android ARM64 optimized APK, runs existing localization/content checks, and retains the proven 2 GiB `ccache` settings. `app/build.gradle` and package signatures are unchanged. Source `gpu/presentation_engine.cpp` and `gpu/pipeline_cache_manager.cpp` remain byte-identical to the compiled FPS-rollback baseline.

Do not claim an FPS improvement or completed Switch OS feature from a passing compilation. Preserve saves before replacing signed APKs.
