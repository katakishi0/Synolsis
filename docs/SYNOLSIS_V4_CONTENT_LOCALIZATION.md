# Synolsis Emu V4 — game content management and localization

## Delivered source

Branch: `feature/synolsis-v4-ptbr-updates`. Android package ID: `org.synolsis.emu`. Core: Strato/Skyline, unchanged licensing and upstream credits.

### Language

- **Português (Brasil) (`pt-BR`)** is selectable from Android Settings and Synolsis preferences.
- The core 298 strings in `values-pt-rBR/strings.xml` cover most of the original English frontend. More UI-specific strings and arrays live in `values-pt-rBR/synolsis_ui_strings.xml`, `synolsis_arrays.xml`, `synolsis_native_controls.xml` and `synolsis_game_content_strings.xml`.
- Default `values` resources remain English. Other installed language entries remain exposed, but strings without translations fall back to English (not automatically translated).
- An XML resource may only be defined once per locale. Duplicate translated arrays/strings were removed from the consolidated branch.

### Per-game NSP update/DLC package manager

- The game dialog has **Updates** and **DLCs** actions to open `GameContentManagerActivity` for a concrete 16-hex-digit title ID.
- Android Storage Access Framework (`ACTION_OPEN_DOCUMENT`) imports user-selected `.nsp` files into the emulator's private storage `filesDir/synolsis-content/<TITLE_ID>/{updates,dlc}`.
- `GameContentStore` validates PFS0 magic, header sizes, file table limits, individual file bounds, file names, and NCA presence before atomically renaming a completed copy. Invalid packages or insufficient storage are rejected; incomplete temporary files are cleaned.
- Imported files can be listed and deleted. No Nintendo game, firmware, key or copyrighted files are bundled.
- **Crucial:** this stage **does not actually install or activate** an update/DLC inside Horizon OS. Its front-end explicitly reports **stored, not mounted**. It does not validate content meta type or TitleID against decrypted CNMT yet.

### Shader cache management

- A per-game **Clear Shader Cache** action removes only that title's caches under the current or legacy `graphics_pipeline_cache` and `vk_graphics_pipeline_cache` trees. It is protected by a confirmation dialog and does not follow symlinks.
- The game should be closed first; the next launch may stutter while re-creating the cache. Other games, saves and shader replacement files are not deleted.

## Why game updates are not fully enabled yet

The existing `NspLoader` opens a base Program NCA + Control NCA. Applying Switch updates usually requires CNMT content metadata, patch Program NCA selection and BKTR RomFS delta overlays. The present Strato `vfs/nca.cpp` recognizes BKTR's encryption enum but does not apply the BKTR patch redirection tables. DLC requires mountable content storage plus `fsp-srv` and `aoc:u` integration; `IAddOnContentManager::CountAddOnContent` and `ListAddOnContent` currently return zero. A file-picker alone must never claim compatibility.

Tracked backend implementation: [Native update & DLC issue #15](../../issues/15). Other unfinished subsystems: QLauncher issue #9, graphics features issue #10 (actual internal scaling, NVDEC, frame generation, etc.).

## QA checklist

1. Select **Português (Brasil)** and restart app. Verify library, driver manager, performance, controller, save and content labels are Portuguese.
2. Open a game info sheet. Select **Updates** or **DLCs**; choose a *locally owned* well-formed NSP and check that it appears in the title-specific list with file size.
3. Test a non-NSP file, a damaged PFS0 index, insufficient space, and rejection of bad 16-digit title IDs. Confirm the app never claims the patch was applied.
4. Delete the staged NSP and confirm only that file is removed.
5. Clear the shader cache with the game stopped. Confirm other games and saves stay intact; test next launch rebuild.
6. Compare default and pt-BR locales, tablet/landscape game dialog scrolling and the selected Turnip/Adreno Vulkan backend.
7. Wait for **GitHub Actions ARM64 optimized** build to finish before distributing an APK. A green resource smoke test alone does not establish runtime correctness.
