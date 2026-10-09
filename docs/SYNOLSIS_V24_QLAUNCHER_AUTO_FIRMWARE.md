# Synolsis V24 — QLauncher autodiscovery from installed user firmware

## Implemented

The original V18 experimental bootstrap requested the QLauncher Program NCA via the Android document picker, even when firmware was already imported through `FirmwareImportPreference`. That is not the intended normal UX.

- `loader_jni.cpp` adds native `findInstalledQLauncherNca(firmwarePath, keysPath)`. It enumerates the **same installed firmware registered directory** used by the existing firmware version/font import code, opens NCA content with the user's existing KeyStore, checks exact program ID `0100000000001000`, Program content type, decrypted ExeFS and required files `rtld`, `main`, `main.npdm`. Broken, non-program, or non-matching NCAs are skipped rather than selected by filename. Returns only a sanitized **basename**, never a content path provided by a guest.
- `SynolsisSettingsHubActivity.kt` starts this scan asynchronously on the I/O dispatcher when the QLauncher card is tapped, validates that the result's canonical parent is the imported firmware directory, and starts the usual `EmulationActivity` `RomFormat.NCA` flow automatically.
- A non-exported, narrowly scoped Android `FileProvider` exposes only `switch/nand/system/Contents/registered/` from app-specific external/internal storage to the separate emulation process with `FLAG_GRANT_READ_URI_PERMISSION`. This avoids forcing the user to copy an NCA into the Documents provider or to pick one manually.
- If no compatible QLauncher executable is found, a localized toast explains that the firmware may be missing/incompatible and launches the **previous manual SAF picker as fallback**.
- UI labels are updated in English and pt-BR.

## Not implemented

This is a **real native+Android automatic launch flow**, not complete Horizon home-menu emulation. Missing services and background title/application management may still cause QLauncher to crash/exit. No firmware or keys are bundled and the installed firmware is never modified.

This also does not change FSR1 pipeline dispatch, true guest-internal rendering scale, GPU frame interpolation, NVDEC host1x/VIC plumbing, or game FPS. The previously observed V20 Turnip flicker is being diagnosed separately by workflow `synolsis-v20-turnip-flicker-fix-ci.yml`.

CI `.github/workflows/synolsis-v24-qlauncher-auto-ci.yml` must finish Kotlin/ARM64, verify FileProvider/scanner wiring, then publish directly to GitHub Releases (Actions Artifacts quota remains full). Tests cannot prove QLauncher boots without a real, compatible, user-supplied firmware and missing services implemented.
