# Patch provenance and testing

These patches are **unmerged candidates** for the pinned `upstream/strato` commit. Nothing here implies that a game launches or that a patch is compatible with all Android devices.

| Patch | Origin and authorship | License and scope | Safety review |
|---|---|---|---|
| `pr-252.patch` | [Strato PR #252](https://github.com/strato-emu/strato/pull/252), PabloG02; pinned PR head `bad297b824c266b46bac0c1e04e313aaa8010d98` | Underlying Strato files retain their original licenses. Kotlin/C++ firmware error-handling changes. | Check first; outdated keys must not silently overwrite existing firmware. |
| `pr-291.patch` | [Strato PR #291](https://github.com/strato-emu/strato/pull/291), Ishan09811; pinned PR head `8c2a6384fa8f9d523c867d2abded3182332a272f` | Underlying layout resource remains subject to Strato license. | Check first; inspect portrait and landscape constraints for every screen size. |
| `pr-9001.patch` | Skyline-Strato-Android experimental patch, based on [upstream `gpu.cpp`](https://github.com/strato-emu/strato/blob/ae1566a48285816a87e81d4aeb40bd2f4e56e60b/app/src/main/cpp/skyline/gpu.cpp) | Modified file carries **SPDX-License-Identifier: MPL-2.0** and Skyline contributors' copyright, preserved on all redistributed versions. | Detect a null Vulkan library handle or missing `vkGetInstanceProcAddr` and report a diagnostic instead of passing an invalid pointer to Vulkan-Hpp. Does not add Vulkan device compatibility by itself. |

## Reproducibility

The patch verifier works against `upstream/strato`. It refuses a dirty worktree, rejects binaries/renames/path traversal, and checks compatibility before writing.

```bash
git clone --recursive https://github.com/vps-generator/Skyline-Strato-Android.git
cd Skyline-Strato-Android
python tools/verify_patches.py --pr 252
python tools/verify_patches.py --pr 291
python tools/verify_patches.py --pr 9001
```

To apply one patch in a fresh checkout:

```bash
python tools/verify_patches.py --pr 9001 --write
```

For a new experiment, use a separate clean upstream checkout and verify source, CPU ABI, GPU vendor, Android version and crash logs.
