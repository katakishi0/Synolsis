# Workflow maintenance — 2026-10-09

The active GitHub Actions workflows are:
- `tooling-ci.yml`: Python tests / patch checks on main.
- `mirror-strato-core.yml`: manual source snapshot refresh (upstream-sensitive).
- `build-standalone-core.yml`: manual unmodified Strato baseline build.
- `synolsis-v25-turnip-skip-ci.yml`: pinned rollback/control APK.
- `synolsis-v27-compat-ci.yml`: pinned V27 comparison APK.
- `synolsis-v28-core-diagnostics-ci.yml`: current V28 diagnostic APK.

Historical workflows were removed from .github/workflows to stop offering obsolete builds. **No source branches or releases were deleted.** Git retains each workflow's complete history: `git log --all -- .github/workflows/<filename>`. To recover an old workflow, `git show <commit>:.github/workflows/<filename>`.

Removed workflow files:
- `synolsis-driver-fingerprint-ci.yml`
- `synolsis-v10-cnmt-owner-ci.yml`
- `synolsis-v11-bktr-aoc-ci.yml`
- `synolsis-v13-bktr-physical-ci.yml`
- `synolsis-v15-vulkan-present-ci.yml`
- `synolsis-v16-fsr-shaders.yml`
- `synolsis-v17-mediacodec-ci.yml`
- `synolsis-v18-qlauncher-ci.yml`
- `synolsis-v19-framegen-ci.yml`
- `synolsis-v20-shader-history-ci.yml`
- `synolsis-v20-turnip-flicker-fix-ci.yml`
- `synolsis-v22-vulkan-compute-ci.yml`
- `synolsis-v24-qlauncher-auto-ci.yml`
- `synolsis-v5-ccache-speed-test.yml`
- `synolsis-v5-ccache-warm-verify.yml`
- `synolsis-v5-english-perf-ci.yml`
- `synolsis-v5-fps-regression-ci.yml`
- `synolsis-v6-content-render-ci.yml`
- `synolsis-v7-native-stability-ci.yml`
- `synolsis-v8-cnmt-ci.yml`
- `synolsis-v9-cnmt-manifest-ci.yml`

## Current technical limitations
V28 introduces a correction to Vulkan staging texture layout transitions and transfer memory visibility. These are code changes awaiting device-side validation; they do not prove that NieR:Automata flicker is fixed. Firmware QLauncher discovery reports why scanning failed, but a functional HOME Menu still requires additional HOS service emulation. The old presentation scale (50%/75%) is not a guest internal resolution option and remains intentionally disabled. A true internal render-scale path needs coordinated render target allocation, viewport/scissor transformation, texture sampling, depth buffers, transfer and readback support.
