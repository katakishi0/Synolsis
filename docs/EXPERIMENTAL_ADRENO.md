# Experimental Adreno / Turnip settings — implementation & source audit

Status: first implementation is isolated in [`feature/experimental-adreno-settings`](../../tree/feature/experimental-adreno-settings) and must pass Android compilation before testing. It does **not** replace the Strato/Skyline engine.

## Source references (2026-10-08)

- **Eden upstream:** https://git.eden-emu.dev/eden-emu/eden, community Nintendo Switch emulator; GPL-3.0-or-later. Android settings, shader pipeline, render scale and frame pacing are architectural references.
- **STORM_SWITCH:** https://github.com/ReiKatari/STORM_SWITCH, a separate Eden/Citron-derived emulator whose September 2026 release notes describe Adreno Vulkan stability, device temperature protection, per-game profiles, frame pacing, mod UI, and video decode changes.
- **Nyushu:** a Nintendo Switch emulator under that name is described publicly, but we have **not** independently verified a current, suitable, licensed source repository for the Android app. Do not import code from an unrelated GitHub account or rehosted binary.
- **Our engine:** pinned Strato/Skyline, source and recursive native libraries stored in our own [`strato-full-core`](../../tree/strato-full-core) branch. Modified source files retain MPL-2.0 copyright notices.

All references are for feature evaluation, not direct copying of incompatible Eden/Yuzu/Citron rendering code into Strato.

## Implemented now: actual Experimental screen

The Android `emulation_preferences.xml` now contains a single **Experimental** category with the eight controls visible inline; the separate `ExperimentalSettingsFragment`, its XML and the dead navigation item were removed. Global and per-game fragments use a shared driver-capability helper. Settings persist through `EmulationSettings`/AndroidX preferences and reach libskyline through `NativeSettings` and `AndroidSettings`.

| Option | Backing engine value | Risk |
|---|---|---|
| GPU executor concurrency (1–6) | `executorSlotCountScale` | Too many slots can consume more RAM |
| GPU executor flush threshold (0–1024) | `executorFlushThreshold` | Incorrect batching can affect frametime |
| Free guest texture memory | `freeGuestTextureMemory` | Low-RAM helper; occasional game instability |
| Force maximum GPU clocks | `forceMaxGpuClocks` | Heat and power; disabled if device lacks support |
| Direct Memory Import | `useDirectMemoryImport` | Proprietary Adreno driver only; disabled in UI and native bridge with a selected custom Turnip |
| Fast GPU readback | `enableFastGpuReadbackHack` | May cause graphics bugs |
| Fast readback writes | `enableFastReadbackWrites` | Requires fast GPU readback toggle |
| Disable subgroup shuffle | `disableSubgroupShuffle` | Major risk of corrupted shaders |

Existing settings like GPU driver selection, portable/TV mode, vsync-related triple buffering, shader cache, audio, and region remain in the original main categories. The experimental page uses *native-backed* options; no fake FPS booster toggles were added.

If using per-game settings, first activate `use_custom_settings` for that game. Test **one toggle at a time** and restart the game to ensure the native state is applied.

## New capabilities not yet implemented

- Frame generation, frame skipping, actual 0.5×–0.75× **internal** resolution changes, FSR 1.0, configurable Vulkan present modes, asynchronous shaders, host NVDEC hardware decode, and CPU multicore overrides need actual engine changes and device validation. A UI switch alone is not an implementation.
- Storm's thermal emergency pause is an interesting safety feature, but needs Android thermal APIs and a tested pause lifecycle before exposing it.
- “Fast Render Distance” is game-engine-specific and not generally controllable by this emulator.
- “Fast GPU Time” is not a proven FPS optimization for every title.

## Hardware test target

**POCO X5 Pro 5G / Snapdragon 778G / Adreno 642L / 6 GiB RAM / custom Turnip driver**. Benchmark the NieR:Automata title menu and in-game scenarios separately using the same ROM/game version, firmware, Turnip version and ambient temperature. Track average FPS, 1% lows, memory pressure, graphics corruption and crashes.

## Build

An isolated Actions job in [`experimental-adreno-ci.yml`](../.github/workflows/experimental-adreno-ci.yml) checks every Experimental preference has a real native mapping, validates XML and attempts to build `:app:assembleDevDebug`. On success the experimental APK is uploaded as a 30-day artifact.

No Nintendo keys, firmware or ROM data are part of this patch.
