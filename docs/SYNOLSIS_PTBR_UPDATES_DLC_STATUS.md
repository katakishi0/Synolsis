# Synolsis Emu — Brazilian Portuguese and Update/DLC roadmap

## Scope / implementation status

Build source: `feature/synolsis-ptbr-content-manager`, built on the earlier Strato native Adreno frame-skip and shader optimization branch. 2026-10-08.

### Working Android frontend implementation

- **Português (Brasil)** added to `BuildConfig.AVAILABLE_APP_LANGUAGES` and `res/xml/locales_config.xml`; Settings -> Theme & colors -> App language now exposes pt-BR. Users whose Android is in Brazilian Portuguese automatically get localized resources.
- `values-pt-rBR/strings.xml` translates 298 of 304 base `strings.xml` keys. Remaining fallback is intentional for app name/username alias and gamepad face button letters (A/B/X/Y). Copyright/third-party credit names are preserved.
- All original Synolsis Material3 UI labels, GPU settings, FPS hacks, per-game settings, controller settings, warnings, errors, file picker descriptions, base theme arrays, Vulkan presentation/frameskip/async pipeline worker options are localized. The English fallback no longer contains UI strings inadvertently written in Portuguese.
- The game bottom sheet has a **Updates / DLC** action that opens `GameContentActivity` using the selected game's actual 16-digit title ID.
- Android Storage Access Framework (`ActivityResultContracts.OpenDocument`) imports uncompressed NSP archives one at a time into `getExternalFilesDir()/switch/synolsis-content/<GAME_ID>/{updates,dlc}/`. Copying is streamed using a 1 MiB buffer in `Dispatchers.IO` to avoid allocating multi-gigabyte packages on the Java heap.
- Each import validates the PFS0 magic, file count, string table bounds and minimum header size. Invalid/truncated inputs are rejected, incomplete copies deleted. Packages receive random names, maintain original names/size/status as JSON metadata, and can be listed and removed per game. The original source file is never deleted.
- Preserve selected Update/DLC category on Activity recreation. No NSP decrypting keys are stored in these metadata files.

### IMPORTANT: updates/DLC are NOT yet applied during game execution

**Import success is not game-update installation.** Current native Strato `IAddOnContentManager` still returns zero AOC entries; `IFileSystemProxy::OpenPatchDataStorageByCurrentProcess` still returns `EntityNotFound`. The game loader currently selects the base program NCA from a single NSP and has no complete update CNMT registration/BKTR patch overlay loader or per-title DLC mounting pipeline. Files imported by Android are intentionally tagged `native_status=staged_not_mounted` and the UI explicitly tells the user they do not modify games yet.

Do **not** describe this as complete working DLC/update support in release notes. No proprietary games/keys/firmware are distributed.

### Native engineering required for complete Update/DLC support

1. Parse PFS0/NSP and NCA **CNMT** metadata (type: Application, Patch, AddOnContent), validate base application program ID and required update version/key generation against imported console keys; reject mismatched title IDs *before* marking as installed.
2. Build a persistent content index, mapping base program ID to known patches and AOC title IDs, account for update/DLC content dependencies and integrity hashes. Store NCA content in registered storage under correct Nintendo-like filesystem naming/metadata.
3. Overlay patch ExeFS and ROMFS correctly, including **BKTR patch delta relocation/subsection tables**, proper decryption, program/control version, code loading, and FS `OpenPatchDataStorageByCurrentProcess`.
4. Implement `aoc:u`/`aoc:*` commands `CountAddOnContent`, `ListAddOnContent`, mount status, availability and change event; implement `OpenDataStorageByDataId` to serve mounted DLC RomFS by AOC ID, not a dummy count. Validate against homebrew probes and a real dumped title.
5. Add user-facing enable/disable/remove/install status only when the above is verifiably active. Include per-game update version and DLC list, control data labels and rollback.
6. Regression test save formats, updates with multiple DLC, encrypted NCA, standby/resume, game launch with and without content.

### Advanced feature matrix, based on actual source

| Feature requested | Status | Details |
|---|---|---|
| Android UI, extra themes, custom Turnip | Present | Native-backed Strato settings, original Material UI |
| pt-BR / other app languages | **pt-BR greatly expanded** | 298/304 English base keys, plus custom UI resources and translated arrays. Other translated languages remain as upstream supported. |
| FPS Hack (executor/readback) | Experimental implementation | Uses real native settings; no guarantees of speed gains. |
| Vulkan present modes FIFO/Mailbox/Immediate | Experimental native implementation | Detect capabilities; fallback FIFO. |
| Presentation frame skipping | Experimental native implementation | Skips host output, preserves guest callbacks; **does not increase simulation FPS**. |
| Parallel Vulkan pipeline compiler worker settings | Experimental native implementation | Auto/1/2/4, subject to device-specific compatibility. Not the same as all shader translation being asynchronous. |
| Pipeline caches, bounded driver binary caches | Implemented, requires device benchmark | Driver-identity isolation, 256 MiB Vulkan cache cap, staged write batching, transactional merge on prior branches |
| Per-game update/DLC import | **Android storage/catalog implemented** | Native NCA application not yet implemented; NOT a DLC installer |
| True internal resolution scaling/FSR | Not implemented | Must change renderer allocations and viewport scaling |
| Full NVDEC (MediaCodec host decode) | Not implemented | Requires Tegra decode services and output synchronization |
| QLauncher HOME Menu | Not implemented | Requires NS/AM/app initialization and HOS system services |
| Frame generation | Not implemented | Requires an optical-flow or motion-vector presentation engine; added latency / GPU memory cost |
| Multi-core CPU/JIT | Existing NCE + executor threads only | Not a replacement with Yuzu's Dynarmic JIT |
| Dynamic resolution/GPU timing hacks | Not implemented | Must intercept guest time queries/feedback honestly and safely |
| Shader backend GLSL/OpenGL, fast render distance | Not implemented | No generic switch in current Vulkan-only Strato architecture |

### Build/test checklist

1. Install new Synolsis APK. **Android package ID is `org.synolsis.emu`**, so migrate data manually from earlier installations when necessary.
2. Open Theme & colors -> App language -> Português (Brasil); verify driver manager, presentation page, game-specific settings and controls show Brazilian Portuguese.
3. Select a game's information sheet -> Updates / DLC; import a valid uncompressed NSP update/DLC from legally sourced console files. Verify it's added to the list and can be removed, and does **not** change game version or DLC availability.
4. Validate without launching a game: import truncated NSP, files with wrong extensions and very small files — they should be rejected without leftover `.pending` files.
5. Check app resources for duplicates and format-string % markers, then confirm Android/Kotlin/C++ compilation and a real APK Artifact. No higher FPS claim until on-device benchmarks match the 24–25 FPS baseline.

See also: `docs/SYNOLSIS_FPS_FRAME_SKIP_VULKAN.md`, `docs/YUZU_UI_FEATURE_PORTING.md` and existing GitHub issues on QLauncher / native graphics.
