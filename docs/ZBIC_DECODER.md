# Experimental HOS 22/23 NSO ZBIC support

The Strato 2024 loader implements LZ4 for compressed NSO sections. Nintendo introduced
a modified Zstandard/BIC codec in HOS 22.0.0, indicated by NSO flag bit 7. Merely
expanding key tables does not decode such content.

## What is staged

[Build workflow](../.github/workflows/zbic-experimental.yml) performs the
following on a **new experimental branch** rather than modifying the stable
`strato-full-core` snapshot before testing:

1. Clone the pinned independent [nxdumptool](https://github.com/DarkMatterCore/nxdumptool)
   source at `ade1a481121498ec649fbc832afe733050790acd`.
2. Vendor the modified Zstd 1.5.7 amalgamation and its headers into
   `app/libraries/zbic`, preserving license notices.
3. Enable `ZSTD_ZBIC_SUPPORT=1`, link into the Strato CMake build, dispatch
   compressed NSO sections by flag bit 7, and preserve LZ4 for earlier formats.
4. Validate the exact decompressed size, returning an error on corruption.
5. Compile an ARM64 Android devDebug APK. **Only if compilation passes,**
   publish the separate `strato-fw23-zbic` branch and upload the APK as an
   experimental GitHub Actions artifact.

This explicitly uses **non-Yuzu/Eden** source. All existing Strato/Skyline
emulation subsystems remain unchanged.

## Licensing and verification

The nxdumptool project uses GPL-3.0-or-later; its included Zstd amalgamation
has upstream Meta BSD-or-GPLv2 notices and independent ZBIC-format modifications.
The experimental integration retains notices and records a pinned source.
Before merging/releasing, review licensing for all modifications and the
embedded libraries rather than assuming the entire dependency has only
a single license.

**Compilation is not game compatibility**: use authorized, local test material
to verify actual ZBIC decompression, segment offsets, errors, and regression on
LZ4 NSO. Check older homebrew and new software and record logs without
revealing keys, firmware archives, or personal file paths.

Sources:
- https://switchbrew.org/wiki/22.0.0
- https://switchbrew.org/wiki/NCA_Format
- https://github.com/DarkMatterCore/nxdumptool
