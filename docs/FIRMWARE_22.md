# Firmware 22.0.0 / keys — compatibility engineering

**Status: experimental patch, not yet evidence of end-to-end compatibility.**

Sources:
- [Switchbrew 22.0.0](https://switchbrew.org/wiki/22.0.0): released 2026-03-17; new master-key revision `master_key_15` (hex).
- [Switchbrew NCA format](https://switchbrew.org/wiki/NCA_Format): the NCA header uses **KeyGeneration 0x16** (22 decimal) for HOS 22.0.0; the associated key index is **0x15** (21 decimal).
- [nstool keyset format](https://github.com/jakcron/nstool/blob/development-tip/SWITCH_KEYS.md): keyset syntax and name conventions.

## The baseline bug

Strato's `app/src/main/cpp/skyline/crypto/key_store.h` defined `IndexedKeys128` as an array of only **20** elements, indexed 0–19. The NCA parser computes the key index from the NCA header (`0x16 - 1 = 0x15 = 21`) and previously indexed both the key table and key-area table *without bounds checking*. Loading valid generation-21 keys could cause out-of-bounds memory accesses, independent of the actual keys.

The C++ key-file parser also discarded the final line if the input didn't end with `\n`, and did not trim whitespace/CRLF in all cases.

## Patches

- `patches/pr-2200.patch` updates the key arrays to 32 entries (indexes 0x00–0x1F), introduces bounds checking for parsed indexes and NCA key accesses, and makes reading keyset lines robust. **No key bytes are included.**
- `patches/pr-2201.patch` improves firmware version detection: skips unrelated unreadable NCA archives, reports missing keys separately, and avoids replacing an existing firmware on detected missing-key errors. Also tightens key-file input length/hex validation.
- `patches/pr-2202.patch` is an **optional** follow-up: tolerates unrelated unreadable NCAs and newly named fonts during extraction. It is not included in the first 22.0 APK build until combined compatibility checks and native compilation are confirmed.
- The patches are designed to be applied together to a **clean** `upstream/strato` checkout. CI validates syntax and `git apply --check` before assembling the APK.

## Critical limitations

Firmware 22.0.0 introduced **modified NSO Zstd-based compression** (bit 7 in NSO flags), which requires a separate, substantial decoder implementation. Merely importing compatible key sets and firmware data does not add that decoder or guarantee that newly released games boot.

Other changes since original Strato may require additional IPC services, NCA format/crypto modes and shader features. **Do not label this as complete Nintendo Switch firmware 22.0 support** until firmware import, font loading, content inspection and game launch are tested on actual devices with authorized materials.

## How to test (without sharing sensitive keys)

1. Build the **firmware-22** preset using [Experimental Android APK](../.github/workflows/build-apk.yml).
2. Install on ARM64 Android 10+ test device. Import personally obtained `prod.keys`, then firmware archives you are entitled to use.
3. Attempt firmware version detection and launch homebrew. Record whether error is `MissingHeaderKey`, `MissingKeyArea`, `ParsingError`, NSO decompression or another specific failure.
4. **Do not attach or commit prod.keys, title.keys, keys files, firmware contents or ROMs.** Redact sensitive paths and IDs in logs.
5. Compare against baseline build, and record regression results.

## Acceptance gates

- [ ] CI confirms both patch files apply without conflict
- [ ] ARM64 APK builds with both patches enabled (GitHub Actions underway)
- [ ] Keys with recognized 0x15 revision are parsed without memory overrun
- [ ] Firmware version 22.0.0 import succeeds using authorized inputs
- [ ] New/unsupported NSO compression returns a clear error rather than memory corruption
- [ ] Homebrew baseline and previous firmware versions remain functional

Tests without protected key material validate parser bounds/format behavior; real firmware compatibility needs device testing.
