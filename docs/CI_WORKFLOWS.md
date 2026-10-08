# CI / GitHub Actions — Synolsis Emu

The project's workflow list was consolidated on 2026-10-08. The old experimental YAML files were retired **from the active Actions directory** to avoid confusing old versions with the latest build. They remain recoverable through Git history and through branch `chore/cleanup-actions-20261008`'s parent commit.

## Active workflows (4)

| Workflow | Purpose | Operation |
|---|---|---|
| `synolsis-driver-fingerprint-ci.yml` | **Current Synolsis Emu ARM64 APK**, source branch `feature/synolsis-vulkan-driver-cache-id`. UI separated, Adreno/Turnip driver-aware Vulkan pipeline cache, firmware fixes and updated settings. | Run from Actions via **Run workflow**, or edit this workflow on main. Builds `:app:assembleMainlineDebug` (`org.synolsis.emu`). |
| `tooling-ci.yml` | Test Python tools and patch compatibility against the pinned upstream Strato. | Automatic on push/PR to `main`, or manually. No APK. |
| `build-standalone-core.yml` | Recovery build of independent `strato-full-core` branch, not the current Synolsis APK. | Manually in Actions, unless its YAML changes. |
| `mirror-strato-core.yml` | Recreate the vendored independent Strato source branch. **Potentially overwrites its target branch.** | Manual maintenance only; avoid running without backups. The workflow also triggers when its YAML itself changes. |

All APK builds use the appropriate source branch. **The current APK build workflow already caches Gradle and CCache.** Actions upload retention is 30 days for the primary Synolsis APK; compiler cache expiration is governed by GitHub's repository/Actions settings (not identical to artifact retention).

## Retired duplicate/experimental workflows

The following YAMLs were removed from `.github/workflows` after their prototype purpose had been superseded:

- `build-apk.yml`
- `cache-build-test.yml`
- `cached-android-ci.yml`
- `experimental-adreno-ci.yml`
- `synolsis-next-ci.yml`
- `synolsis-separated-settings-ci.yml`
- `synolsis-settings-v2-ci.yml`
- `synolsis-themes-cache-ci.yml`
- `zbic-experimental.yml`

Their historic runs and source commits can still be consulted in the repository history (availability of old Actions artifacts depends on GitHub retention). This does **not** delete emulator source, branches, saves, release assets or the reusable CCache/Gradle entries.

## Recommended development process

1. Develop/test patches on a dedicated source branch; preserve working Android builds.
2. Apply the change to `feature/synolsis-vulkan-driver-cache-id` after validation.
3. Trigger **Synolsis Adreno Driver-Cache ID APK** from Actions (or edit the CI YAML on main if it needs an updated test).
4. Wait for **all** jobs including the upload-artifact step; only then download the new ZIP under Artifacts.
5. Test on Snapdragon 778G / Adreno 642L with a known Turnip version and measure cold/warm cache frame times. Never infer FPS gains from a green build.
6. When the next GPU/core branch replaces the current source branch, update the checkout `ref:` and the CI's invariants together, instead of creating another nearly identical workflow.

For historical restoration: find the deleted YAML at commit `90a8aac549b1da4e48643305a32c0e7b32936937` and restore only what is needed. Do not reintroduce all prototype workflows just to retain history.
