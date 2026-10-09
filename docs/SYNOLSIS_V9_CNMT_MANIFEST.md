# Synolsis Emu V9: Native CNMT content manifest

## Implemented

This branch extends the successfully compiled V8 baseline and keeps its V5 rollback Vulkan presentation behavior.

- `vfs/content_meta.h`: parse content-info records from already decrypted `.cnmt` files. Entries include the 32-byte SHA-256 advertised in CNMT, 16-byte NCA content ID, six-byte little-endian content length, entry type and ID offset.
- The reader rejects out-of-bounds tables, duplicate/zero content IDs, zero-size content, and excessive manifest allocations. It exposes lower-case `<32-character content ID>.nca` filenames and exact application ID matching; never infer ownership from an NSP file's display name.
- `loader/nsp.cpp` reads the records through the existing NCA decryption and PFS0 layers, retains them in `NspLoader::GetContentManifests()`, and warns if a listed NCA filename is not present in the NSP. That check is **filename coverage only**, not a hash/signature verification.
- `tests/cnmt_manifest_test.cpp` adds host-only C++20 tests using fabricated CNMT inputs: owner ID matching, content IDs, 48-bit sizes, expected NCA filename, truncation, duplicate IDs, empty IDs, zero-size data and count caps.

## Deliberately not activated

The imported NSPs in `GameContentStore` remain staged. This branch does **not** mount update/delta BKTR RomFS, redirect program ExeFS, register installed content in `aocsrv` or fetch game-specific CNMTs from the Java UI.

The SHA-256 in a CNMT table has **not** been verified against the actual NCA bytes. Without that step (and trusted header metadata/key material) the manifest may be forged; it is unsuitable as proof of an installed/authorized update. The user must supply any firmware/keys used for decryption. No copyrighted game data is bundled.

Likewise, working 0.5x/0.75x Vulkan render-target scaling, FSR, QLauncher, NVDEC/MediaCodec, and frame generation remain separate tasks. Merely compiling helper functions does not implement these features.

## CI

`.github/workflows/synolsis-v9-cnmt-manifest-ci.yml` runs the independent C++ parser tests, checks native loader integration, compiles Android/Kotlin + ARM64 and publishes a test APK. Build-time `ccache` settings preserve the proven 2 GiB cache.
