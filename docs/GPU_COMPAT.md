# Vulkan / GPU compatibility investigation (Skyline/Strato)

Status: analysis of **upstream pinned** `strato-emu/strato@ae1566a48285816a87e81d4aeb40bd2f4e56e60b`. This is a research plan, **not** proof of game compatibility.

## Existing Strato architecture

- `app/src/main/cpp/skyline/gpu.cpp` loads a requested custom Vulkan driver via `libadrenotools`. If that fails, it attempts the built-in/system loader and then `dlopen("libvulkan.so")`.
- On failure, the old path passed `dlsym(libvulkanHandle,...)` directly into Vulkan-Hpp. **Patch #9001** detects a missing loader or `vkGetInstanceProcAddr`, logs an error and throws a regular Skyline exception. This improves **failure diagnostics**, not Vulkan extension coverage.
- `app/src/main/cpp/skyline/gpu/trait_manager.cpp` selects traits and quirks from queried Vulkan physical-device features, extension list and driver ID. It includes known proprietary Adreno, Turnip and ARM-related workarounds. A specific PowerVR workaround is not present in this snapshot.
- The Vulkan context requests Vulkan 1.1 (`VkApiVersion` in `skyline/gpu.h`) and instance extensions, including `VK_EXT_debug_report`. Investigate whether this requirement is overly strict on some Android drivers **before** making it optional, because `CreateDebugReportCallback` uses it directly.

## PowerVR track (e.g. Imagination / MediaTek)

1. Use **system Vulkan driver**, not Turnip/AdrenoTools driver packages. Upstream links `libadrenotools` for loader handling, but that does **not** mean a Turnip ICD is compatible with PowerVR.
2. Obtain vendor ID, driver ID, device name, Vulkan API version, extensions, memory types and limits from **Vulkan API queries** (not only Android marketing names).
3. Catch errors when Vulkan layers/instance extensions/device features are unavailable; show actionable diagnostics in app logs.
4. Validate texture formats, render pass compatibility, graphics/compute capabilities, and importable memory with targeted **homebrew** Vulkan tests.
5. Benchmark shader cache and compilation independently on Adreno, Mali and PowerVR devices. Do not enable driver-specific quirks without reproductions and regressions.

## Suggested experiments

| ID | Test | Pass condition |
|---|---|---|
| VK-01 | Missing `libvulkan.so` / entrypoint on a test harness | Clear error rather than null function pointer passed to context |
| VK-02 | Vulkan 1.1 device probing on PowerVR | Devices and required extensions logged; unsupported paths explained |
| VK-03 | Sample game or homebrew 3D scene | Correctness first, then frame pacing and compile stutter |
| VK-04 | Suspend/resume application | Vulkan surface recreated; no leaked/invalid pointers |
| VK-05 | Custom driver selected on incompatible GPU | Safe fallback, not installation instructions for incompatible drivers |

## Logs and privacy

`python tools/gpu_report.py --output gpu_report.json` gathers only basic Android graphic properties. User must review logs before sharing; avoid serial numbers, tokens, paths containing personal information or personal data. Detailed Vulkan GPU information should be added to the application itself in a future build.

No compatibility guarantee without a built APK and test device.
