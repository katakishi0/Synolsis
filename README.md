# Skyline-Strato-Android

**A Skyline/Strato-based Nintendo Switch emulator research and improvement project for ARM64 Android.**

> Current status **v0.2 — source link + patch candidates + CI preparation**.
> The complete upstream core is linked as a pinned Git submodule. Patches are **not merged** into the core by default. No APK has been verified yet.

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

### Candidate fixes

We have extracted the original diffs from Strato pull requests:

| PR | Candidate | Files | Status |
|:--:|---|---:|---|
| [#252](https://github.com/strato-emu/strato/pull/252) | Avoid firmware import crash when keys are outdated | 3 | Reviewed as candidate, untested on Android |
| [#291](https://github.com/strato-emu/strato/pull/291) | Game detail dialog scroll/layout fix in landscape | 1 | Reviewed as candidate, untested on Android |

Try without changing the emulator:

```powershell
python tools/verify_patches.py --pr 252
python tools/verify_patches.py --pr 291
```

Apply **only one at a time, after making a clean backup**:

```powershell
python tools/verify_patches.py --pr 252 --write
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
- `patches/`: source diffs for candidate fixes (GPL provenance documented above).
- `tools/verify_patches.py`: strict local check and controlled apply.
- `project.py`: independent clone and SHA-pinned PR staging tool.
- `tools/gpu_report.py`: read-only Android GPU diagnostics.
- `tests/`: offline regression tests for tooling and patch staging.
- `docs/`: research, risk decisions, milestones.

**No ROMs, firmware, console keys, prod.keys, commercial game images or copyrighted assets are included.** Test with homebrew and files you are authorized to use.
