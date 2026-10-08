# Skyline-Strato-Android

**A Skyline/Strato-based Nintendo Switch emulator research and improvement project for ARM64 Android.**

> **Active experimental build: Synolsis Emu** — standalone Strato/Skyline code, local firmware 22/23 and Adreno/Vulkan experiments.
> The `main` branch holds the toolkit/workflows and a pinned upstream reference; self-contained native source lives in `strato-full-core` and feature branches. APK build artifacts are created by GitHub Actions after passing compilation; each new feature build requires separate confirmation.

## Synolsis Emu (new experimental app branding)

Source branch: **[feature/synolsis-themes-cache](../../tree/feature/synolsis-themes-cache)** — Android name/header and icon updated to **Synolsis Emu** (application ID unchanged; Android upgrades still require the **same APK signing certificate** — back up saves before installing a CI debug APK), five additional Material 3 themes (Midnight, Purple, Emerald, AMOLED Red, Amber), and actual Strato Vulkan/pipeline cache fixes:

- Merge `.staging` pipeline cache data only once, refuse truncated headers.
- Separate pipelines for Turnip/custom and stock Adreno driver selection.
- Fallback to FIFO when MAILBOX Vulkan present mode is unavailable; clamp triple-buffer requests when needed.
- Existing Adreno Experimental controls and firmware 22/23 ZBIC work remain in the branch.

See [technical research and benchmarks](../../blob/feature/synolsis-themes-cache/docs/SYNOLSIS_ADRENO_CACHE_RESEARCH.md), [FPS tracking #8](../../issues/8), and the [Synolsis APK CI](../../actions/workflows/synolsis-themes-cache-ci.yml).

**No speedup is guaranteed.** Builds must finish successfully and run on the Snapdragon 778G/Adreno 642L before any FPS comparison.

## Cached builds (Gradle + native ccache)

Run the [cached ARM64 APK build](../../actions/workflows/cache-build-test.yml) to build from the **self-contained** `strato-full-core` or experimental `strato-fw23-zbic` branch. GitHub caches Gradle downloads and C/C++ compile objects; the build includes a cache-hit report and keeps APK artifacts for 30 days.

**The dependency-cache retention is a separate GitHub repository setting** (normally 7 days). See [how to enable 30-day cache retention](docs/CI_CACHE.md); the workflow itself cannot change that setting.

## Independent full Strato core branch

**[Browse `strato-full-core`](../../tree/strato-full-core)** — all native emulator sources, our experimental 22.x/23.x key generation and firmware patches, and *recursive third-party source dependencies* are committed directly into this repository. The branch has **no Git submodule pointers**. This preserves the build's source materials even if original upstream repositories disappear.

```powershell
git clone --branch strato-full-core --single-branch https://github.com/vps-generator/Skyline-Strato-Android.git
cd Skyline-Strato-Android
# You now have full Strato sources + vendored native dependencies in app/libraries/
./gradlew :app:assembleDevDebug
```

The upstream reference SHA, origin and license notices are recorded in the branch's `UPSTREAM_PROVENANCE.md`. The original Strato GitHub Actions workflows are omitted due to GitHub App permission limitations; our own build workflow lives on `main`.

**Current Nintendo Switch firmware is 23.0.1 (September/October 2026)**. KeyGeneration `0x17` corresponds to master key index `0x16` and fits our patched 32-entry key tables; this is key-format support, **not full firmware execution compatibility**. See [firmware compatibility scope](docs/FIRMWARE_COMPATIBILITY.md) and the standalone [APK build workflow](../../actions/workflows/build-standalone-core.yml).

### Source and licensing

- **Emulator core:** [Strato](https://github.com/strato-emu/strato), continuation of Skyline, pinned at commit `ae1566a48285816a87e81d4aeb40bd2f4e56e60b`. View it in [`upstream/strato`](upstream/strato).
- Source is incorporated by **Git submodule**, not copied/squashed into unrelated history. Fetch it with `--recursive`.
- **Not** a Yuzu/Eden fork. Strato itself includes subcomponents with separate history, such as its Yuzu-derived shader compiler. These remain under their original licenses.
- Emulator core and contributed patches must retain original GPL-3.0-or-later obligations and contributor notices. Project tooling is MIT.

### Clone and inspect

**Windows PowerShell (Git + Python 3.9+):**

```powershell
git config --global core.symlinks true
git clone --recursive https://github.com/vps-generator/Skyline-Strato-Android.git
cd Skyline-Strato-Android
python -m unittest discover -s tests -v
git submodule status
python project.py patches
```

If cloned without submodules:

```powershell
git submodule update --init --recursive
```

### Firmware 22.0 experimental compatibility

The HOS 22.0 key update is staged and covered by CI:

- [`pr-2200.patch`](patches/pr-2200.patch): accommodate modern key index 0x15, validate array bounds and parse CRLF/last-line keyset entries.
- [`pr-2201.patch`](patches/pr-2201.patch): avoid firmware detection crashes, report mismatched keys and validate key inputs.
- [Firmware 22.0 scope and limitations](docs/FIRMWARE_22.md).
- [Firmware 22.0 development issue](../../issues/4) and [ZBIC NSO compression follow-up](../../issues/5).

The **Experimental Android APK** workflow defaults to building the `firmware-22` preset with both patches. **Patch checks passing is not a confirmation of firmware 22.0 operation in games.** HOS 22 uses a modified ZSTD/BIC NSO compression which still needs a decoder. No firmware or keys are provided.

### Candidate fixes

We have extracted the original diffs from Strato pull requests:

| PR | Candidate | Files | Status |
|:--:|---|---:|---|
| [#252](https://github.com/strato-emu/strato/pull/252) | Avoid firmware import crash when keys are outdated | 3 | Reviewed as candidate, untested on Android |
| [#291](https://github.com/strato-emu/strato/pull/291) | Game detail dialog scroll/layout fix in landscape | 1 | Reviewed as candidate, untested on Android |
| [#9001](patches/pr-9001.patch) | Guard missing Vulkan loader/entrypoint and emit useful error | 1 | Original Skyline-Strato-Android candidate; patch check passes on pinned base, Android runtime untested |

Try without changing the emulator:

```powershell
python tools/verify_patches.py --pr 252
python tools/verify_patches.py --pr 291
python tools/verify_patches.py --pr 9001
```

Apply **only one at a time, after making a clean backup**:

```powershell
python tools/verify_patches.py --pr 252 --write
# For the Vulkan loader error-handling candidate, use a separate clean checkout:
# python tools/verify_patches.py --pr 9001 --write
```

This modifies your local `upstream/strato` worktree (a pinned submodule). It does **not** publish the fix, does **not** prove that the emulator builds, and does **not** automatically apply #291. For an independent patched branch with Git attribution, use `project.py apply --repo <separate Strato clone> --pr 252 --write`.

### Android build prerequisites

The upstream `app/build.gradle` requires **Android SDK 34**, **NDK 26.1.10909125**, **CMake 3.22.1+**, **Java 17**, Android **minSdk 29**, and an **arm64-v8a** device. The build uses Gradle 8.2. See [Strato BUILDING.md](https://github.com/strato-emu/strato/blob/master/BUILDING.md).

```powershell
cd upstream/strato
./gradlew :app:assembleDevDebug
```

On GitHub, workflows under [Actions](../../actions) perform tool tests and can try compiling a debug APK. Builds require Android SDK downloads and large recursive upstream dependencies. An **Actions artifact is not a release**; if build fails, inspect job logs.

### GPU diagnostics

Run when a device is connected by ADB:

```powershell
python tools/gpu_report.py --output gpu_report.json
```

For PowerVR GPUs, favor the system Vulkan implementation. **Turnip and custom Adreno drivers are not a PowerVR fix.** An ADB vendor guess alone is not proof of Vulkan compatibility.

### Project structure

- `upstream/strato/`: full, pinned Skyline/Strato emulator core (submodule).
- `patches/`: source diffs, authorship and file-specific licenses described in [patches/README.md](patches/README.md).
- `tools/verify_patches.py`: strict local check and controlled apply.
- `project.py`: independent clone and SHA-pinned PR staging tool.
- `tools/gpu_report.py`: read-only Android GPU diagnostics.
- `tests/`: offline regression tests for tooling and patch staging.
- `docs/`: research, risk decisions, milestones.

**No ROMs, firmware, console keys, prod.keys, commercial game images or copyrighted assets are included.** Test with homebrew and files you are authorized to use.
