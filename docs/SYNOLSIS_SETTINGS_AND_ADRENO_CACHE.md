# Synolsis Emu — separated settings and Vulkan Adreno/Turnip cache identity

Target: **POCO X5 Pro 5G, Snapdragon 778G, Adreno 642L, 6 GB RAM**. Based on Skyline/Strato, preserving upstream credits.

## UI separation

The Synolsis settings hub now routes to the existing, *functional* settings and tools instead of mixing them into a single Advanced page:

| Hub card | Screen implementation |
|---|---|
| Advanced Settings | `SettingsActivity` -> `GlobalSettingsFragment` with `emulation_preferences.xml` only |
| GPU Driver Manager | Direct `GpuDriverActivity` (install/select system/Turnip driver) |
| Controllers | `SettingsActivity` -> `input_preferences.xml`, players 1–8 |
| Theme and Color | `SettingsActivity` -> `app_preferences.xml`, **appearance** category only |
| Firmware and Data | `SettingsActivity` -> `app_preferences.xml`, **content** category only (keys/firmware/import) |
| About and Licenses | `SettingsActivity` -> `credits_preferences.xml` |

The advanced graphics category no longer contains `GpuDriverPreference`, so selecting a modified Vulkan driver cannot appear in two unrelated places. A **per-game GPU override** remains in `custom_game_preferences.xml`, dependent on `use_custom_settings`.

The library's old top Settings icon now opens the hub too. Deep links for theme, firmware, and data go to real existing preferences rather than placeholder screens.

All options continue to share the existing Strato Android preference stores. Existing emulator-core graphics settings have **not** been relabeled as GPU driver management.

## Vulkan cache identity update

The previous host cache directory was based only on the custom driver *label* (or `system`). Updating Turnip in place without changing its label could reuse data compiled under an older ICD. The updated code combines:

- Selected driver label
- Vulkan physical-device vendor and device IDs
- Vulkan driver and API versions
- Vulkan pipeline-cache UUID

These identifiers are fed through a stable FNV-1a hash to form a safe `system_...` or `custom_...` path for both graphics pipeline caches.

**Expected effects:** safer driver upgrades and fewer driver-mismatch crashes/corrupted caches. The first run with a new driver identity creates a cold cache, so temporary first-run shader stutter is normal. **No FPS increase is promised.**

## Builds

- **UI only:** [Synolsis Clean Settings CI](../../actions/workflows/synolsis-settings-v2-ci.yml), branch `fix/synolsis-settings-separated`.
- **UI + Vulkan keying:** [Synolsis Adreno Driver Cache CI](../../actions/workflows/synolsis-driver-fingerprint-ci.yml), branch `feature/synolsis-vulkan-driver-cache-id`.

Both use cached Gradle/ccache build steps, static XML/native mapping checks and build `assembleMainlineDebug`, producing package ID `org.synolsis.emu`.

## Test checklist

1. Back up old saves and firmware config. Changing the Android package from `org.stratoemu.strato` to `org.synolsis.emu` creates a separate sandbox.
2. Launch the new APK. Tap the bottom **Settings** tab and the toolbar Settings icon: both should open the same hub.
3. Open **Advanced** and check that no GPU driver installer appears there.
4. Open **GPU Driver Manager** and verify the system driver and custom Turnip entries still work.
5. Open **Controllers**, **Theme and Color**, **Firmware and Data**, and **About**; no blank pages or crashes.
6. In a game's custom settings, check per-game GPU driver remains available when `Use Custom Settings` is on.
7. Benchmark NieR:Automata with a fixed Turnip version and scene, then compare a cold and warm cache run. Record memory and FPS over minutes, not one title-screen snapshot.

If compile or runtime tests fail, retain the previous working APK and revert the experimental branch. No Nintendo keys/ROMs/firmware are distributed.
