# Synolsis Emu — separated settings layout

Branch: `fix/synolsis-separate-gpu-driver-settings`. Based on `feature/synolsis-next-ui-and-graphics`, preserving the Skyline/Strato engine, firmware 22/23/ZBIC work, driver-specific Vulkan cache, current palettes, UI navigation, and native-backed experimental settings.

## Why this exists

The previous global **Advanced settings → GPU** category contained the **custom/modified GPU driver manager** as well as Vulkan rendering switches. This duplicates the dedicated **GPU Driver Manager** card in the Yuzu/Nyushu-inspired hub and mixes driver installation with graphics/performance tuning.

## New navigation

**Settings hub**

- **Advanced settings:** Switch system, display, audio, Vulkan runtime, performance/memory and experimental toggles, debug. No driver management entry.
- **GPU Driver Manager:** separate card opens the real `GpuDriverActivity`, for selecting system Qualcomm driver or a supported user-provided Turnip ZIP.
- **Controls:** real controller mapping activity.
- **Theme and colors:** only appearance preferences, with the six Synolsis palettes.
- **Firmware and keys:** only content preferences, scrolled to `firmware`; keys are provided locally by the device owner.
- **Library and data:** only content preferences, scrolled to `document_provider`, with folder picker and saves options.

The Android hub passes the `synolsis_settings_section` extra. The global settings fragment selects the appropriate existing XML preferences: `advanced` = `emulation_preferences.xml`, `appearance` = appearance category from `app_preferences.xml`, `content` = content category. Direct legacy launches without a section retain access to all previous settings for backwards compatibility.

## Per-game GPU driver

The `GpuDriverPreference` XML entry moved from global `emulation_preferences.xml` into `custom_game_preferences.xml`. It depends on `use_custom_settings` so that each game can still override the global Turnip driver *when per-game configuration is enabled*. This is not a second driver installer: it launches the original GPU driver selection activity with the game's `AppItemTag`.

## Engine and install identifiers

- Public Android package: `org.synolsis.emu`, handled by Gradle applicationId.
- Original internal Kotlin/JNI namespace remains `org.stratoemu.strato` for binary compatibility with existing Strato native symbols.
- Product name and UI: Synolsis Emu; upstream license/credits retained.
- Existing user apps with the former package ID have separate Android sandboxes. Export saves before switching.

## Validation checklist

The CI workflow [Synolsis Separated Settings and GPU CI](../.github/workflows/synolsis-separated-settings-ci.yml) must verify:

- [x] Exactly one per-game driver override preference, dependent on custom settings
- [x] No global driver preference embedded in advanced graphics
- [x] All hub entry points launch a real activity and category
- [x] No nonnull assertion for a preference missing in standalone appearance/content screens
- [ ] Compile ARM64 APK from this branch
- [ ] Test all navigation routes on a device
- [ ] Test switching from Qualcomm system Vulkan to Turnip and back, including per-game overrides
- [ ] Test firmware import, game-folder selection, and save management
- [ ] Confirm UI changes don't alter the real render backend or measured FPS unintentionally

This task separates functions; it does not claim added frame generation, QLauncher boot, NVDEC host acceleration, or FPS improvements.
