# Synolsis V11 — BKTR relocation engine and local AOC mounts

Source branch: `feature/synolsis-v11-bktr-aoc-runtime`, based on the V10 Android CNMT validator.

## Real features added

- `vfs/bktr_relocation.h`: parse decrypted BKTR indirect relocation buckets (base/patch source offsets, guest virtual offsets). Rejects invalid bucket layouts, flags, table sizes, inverted ranges and reads out of source bounds. `ReadDecrypted` actively stitches spans from independently decrypted base and patch images, including reads crossing base/patch boundaries. Tests use synthetic relocation tables.
- `vfs/nca.h/.cpp`: read actual BKTR PatchInfo in each NCA section header, recover/decrypt a bounded relocation table, and expose presence/parsed relocation count for diagnosis. **Fail closed** on incomplete BKTR support: the old NCA branch incorrectly exposed a BKTR section as a contiguous AES-CTR RomFS. It now refuses this invalid mounting until the BKTR-specific AES-CTR-Ex subsection table is implemented alongside the base RomFS mapping.
- `services/aocsrv/mounted_add_on_registry.h`, `os.cpp`: at game launch, scan at most 128 user-imported, private `synolsis-content/<baseTitleId>/dlc/*.nsp` packages. Use existing NCA key store, user tickets and CNMT parsing. Only mount `Data` NCAs with real decrypted RomFS, matching CNMT application ID and the AOC title ID in the NCA header. Wrong-title, malformed or unreadable NSPs are skipped; no synthetic or staged-only DLCs are registered. Keep sources backed by file descriptors.
- `IAddOnContentManager.cpp/.h`: native IPC now exposes real mounted add-on count, indexes and AOC base ID (commands 2, 3 and 5); preparation (7) reads mounted status. All counts are backed by mounts rather than strings in a manifest.
- `IFileSystemProxy.cpp`: `OpenDataStorageByDataId` returns a real mounted DLC RomFS for its AOC title ID; otherwise the pre-existing firmware lookup/fallback is used.
- `tests/bktr_relocation_test.cpp`, `tests/aoc_title_id_test.cpp`: checked virtual relocation span reads and game-specific add-on ID mapping.

## Incomplete / known limitations

1. **BKTR patch RomFS is not fully active yet.** Its relocation table parser and decrypted-image stitcher work, but actual update installation still needs decryption with each *subsection* AES-CTR-Ex counter, base RomFS overlay and patched ExeFS mapping. In particular, parsing the relocation table alone is **not** a playable update.
2. **AOC content is from local/user-imported files**. No Nintendo entitlement, signature, content-record SHA-256 or ownership authentication is asserted. Decryption using the user's own keys and an actual Data NCA RomFS are needed; DLCs without readable assets remain unavailable. At present the registry is loaded at game launch, not dynamically refreshed.
3. Actual Vulkan 0.5×/0.75× render-target scaling, FSR, QLauncher, NVDEC to Android MediaCodec, and real frame interpolation are **not completed** in this branch. The geometry-only helper from V6 must not be represented as a working GPU rescale.
4. Preserves the V5 GPU rollback and English-first/optional pt-BR behavior. FPS measurements require a real Android device; CI success doesn't prove an FPS increase.

Use `.github/workflows/synolsis-v11-bktr-aoc-ci.yml` for host unit tests, full Kotlin + ARM64 compile, cached APK artifact.
