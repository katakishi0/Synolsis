# Synolsis V10 — Native CNMT game association at Android import

The previous V9 loader parsed individual NCA content records but did not let an Android user validate the identity of a staged update/DLC.

## Functional integration

- `loader_jni.cpp`: new `GameContentStore.readNativeContentMetadata(fd, keysPath)` JNI entrypoint inspects a standalone update/DLC NSP using the existing encrypted NCA/VFS parser. Unlike boot-only `NspLoader`, it **does not require** a Program ExeFS and Control NACP. It reads only `.cnmt.nca` metadata files to avoid processing large program/data files and limits decoded records and file lengths. Uses user-owned keys and tickets. The caller owns/closes the passed file descriptor.
- `GameContentStore.kt`: after streaming and validating an import, query the native inspector on the staged private file, parse the content type and base application ID, and reject wrong-game NSPs and wrong-kind imports (update versus DLC) when that metadata is readable. Reject mixed-game bundles to avoid assigning contents from several games to one title. If metadata or keys are unavailable, retain the old staging behavior **without claiming that the package is verified**.
- Sidecar JSON persists `cnmt_owner_matched` and the parsed type/title, while intentionally retaining `title_id_verified=false`, because NCA signature/content SHA256 verification and trust provenance have **not** been implemented.
- `GameContentManagerActivity.kt` labels packages with readable matching CNMT ownership as **not installed**, and displays errors in English and pt-BR when a package is associated with another game or the wrong content type.

## What it cannot yet do

This is real native **package-to-game validation**, not an installer or HOS content manager. It does not apply ExeFS replacements, BKTR RomFS overlays, register AOC, alter the emulated game, validate Nintendo signatures, or guarantee that all CNMT-referenced NCA hashes match on-disk data. On a device without suitable keys, metadata inspection may return no result; contents are staged **unverified**, never shown as active.

Actual internal Vulkan rendering at 0.5x/0.75x, FSR, QLauncher HLE, Android MediaCodec NVDEC and frame interpolation remain unimplemented. The V5 rollback GPU behavior and ccache build configuration are unchanged.

Build `.github/workflows/synolsis-v10-cnmt-owner-ci.yml` and confirm APK before distribution. Never bundle firmware or production keys.
