# Synolsis V13 — BKTR AES-CTR-Ex physical backing

Based on the compiled V11 AOC + BKTR metadata branch, plus V12 host subsection boundary tests.

## Implemented in native code

- `vfs/bktr_ctr_counter.h` implements the physical patch subsection AES-CTR-Ex initialization vector: bytes 4–7 hold the per-subsection counter, 8–15 hold the absolute NCA byte offset divided by 16. Header remains testable on host C++20 without Android dependencies. Refer to the BKTR NCA format and existing Strato AES-CTR implementation for encryption context.
- `vfs/bktr_ctr_ex.h` adds a read-only VFS `Backing` over encrypted physical patch NCA section data. Each read chooses its actual physical subsection counter, decrypts AES-CTR using the existing Strato `crypto::AesCipher`, handles unaligned guest reads and subsection transitions, protects key state with a mutex and caps temporary storage to 128 KiB. If invalid subsection boundaries prevent safe decryption, it returns a short read rather than inventing bytes.
- `vfs/nca.cpp` creates `bktrPhysicalSection` after both indirect relocation and subsection tables have passed validation. The previous incorrect treatment of BKTR as a guest-visible ordinary CTR RomFS remains disabled.
- `vfs/bktr_merged_backing.h` supplies a true *on-demand* sparse read-only virtual image composition over an already decrypted base section and CTR-Ex decrypted patch section. Each read uses BKTR relocation, without copying the whole image. This is a reusable primitive but NOT yet wired to the app's active RomFS.
- `tests/bktr_ctr_ex_counter_test.cpp` checks byte-order and block counters with C++20 compile-time assertions and runtime inputs; V11/V12 relocation and boundary tests are retained.

## Still NOT working end-to-end

NCA patch updates do not replace a running game's executable or live RomFS yet. Before exposing a merged virtual BKTR storage to guest FS we must verify: the base/patch NCA title IDs + CNMT version; IVFC physical level offsets in *both* NCAs; the intended virtual RomFS data level; and full expected content/section data sizes. All of these offsets differ from simply passing the base `NCA::romFs` backing to the section-relative relocation table.

On-device validation with legally sourced game/update assets is also required. The APK build passing CI does not prove BKTR gameplay, entitlement, NVDEC, actual internal Vulkan scaling, FSR, QLauncher or frame interpolation.

The existing functional AOC native lookup/list and own-content loader, V5 GPU rollback and proven 2 GiB ccache are preserved. No firmware/keys are bundled.

CI workflow: `.github/workflows/synolsis-v13-bktr-physical-ci.yml`, requiring C++20 host tests and full optimized Android ARM64 build.
