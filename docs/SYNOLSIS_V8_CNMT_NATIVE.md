# Synolsis V8 — native CNMT package identity inspection

This branch extends the compiled V7 renderer/FS baseline. It does **not** replace its 1× graphics output or reset Adreno settings.

## Implemented

- `app/src/main/cpp/skyline/vfs/content_meta.h`: safe, zero-copy parser for **decrypted** standard `nn::ncm::PackagedContentMetaHeader` CNMT bytes. It recognizes application (0x80), patch/update (0x81), AOC/DLC (0x82), delta (0x83), and data patch (0x84). Reads the containing CNMT title ID, version, owning base application ID (from correctly typed extended headers), counts and total extended size. Uses checked arithmetic and validates every entry table range before returning metadata; rejects zero app IDs and unknown types. Works on Android C++20 and host compilers.
- `app/src/main/cpp/skyline/loader/nsp.h` / `nsp.cpp`: live `NspLoader` now examines Meta NCAs using existing `vfs::NCA` decrypt/key store and PFS0 CNMT file reads. Saves the package identity summaries in the loader and emits a diagnostic log with the owning base game ID and version. Limits parsing to metadata files of 4 MiB or less. An absent, unsupported or undecipherable CNMT does not misreport an update as mounted, and does not change the base game's executable selection.
- `tests/cnmt_metadata_test.cpp`: C++20 tests of all recognized content meta types and owner-ID offsets, malformed/truncated input, unsupported system content, absurd entry counts and invalid extended-header size.

### Why this matters

A file named `update.nsp` is **not** necessarily a patch for the selected game; the base application ID lives in its decrypted CNMT. The old Java NSP importer only verified the PFS0 container and counted `.cnmt.nca` filenames. This native work supplies the metadata needed for title-aware update/DLC selection without pretending that imported content has already been installed.

## Still not implemented

- Native import-time matching of every staged package to the library game title ID, cryptographic validity/ownership checks, BKTR patch overlay, game-update ExeFS replacement, AOC filesystem mounting/registration, or actual update/DLC launch. The current metadata reader is part of `NspLoader`; the Java staging screen still labels packages as **not mounted**.
- Actual internal Vulkan resolution scaling and FSR, QLauncher system-menu HLE, NVDEC MediaCodec decoder and frame interpolation. Preserve all existing working features and keep future additions behind actual native readiness.
- No keys/firmware are bundled. The user's own keys are necessary for encrypted metadata.

The new CI `.github/workflows/synolsis-v8-cnmt-ci.yml` must compile and execute the host parser tests and complete the full ARM64 Strato APK build with persistent CCache before this stage can be called successfully built.
