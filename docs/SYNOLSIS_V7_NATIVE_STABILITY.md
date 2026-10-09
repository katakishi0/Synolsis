# Synolsis V7 — real native stability work

Base: compiled V6 NSP integrity / scale-geometry branch. Keep V5 rollback graphics presentation + shader writer semantics unchanged until real device FPS tests.

## Implemented

1. **FS service storage reads** (`services/fssrv/IStorage.cpp`): honor the HOS IPC requested byte count instead of reading the full outgoing buffer, handle missing buffer, clamp to actual backing length, return invalid offset on attempts past EOF. No copy may exceed available backing or output buffer.
2. **Installed firmware lookup** (`services/fssrv/IFileSystemProxy.cpp`): ignore unrelated non-NCA entries, skip malformed or unreadable system NCAs without crashing the entire data-storage request, allow Android-shipped fallback RomFS to be queried when the imported firmware is partial, return the filesystem `EntityNotFound` code instead of constructing null IStorage for missing files. These help incomplete local firmware handling, but **do not implement QLauncher** or remove the need for user-provided firmware/keys.
3. **Real Vulkan scissor validation** (`gpu/interconnect/maxwell_3d/active_state.cpp`): replace signed narrowing of `xMax-xMin` / `yMax-yMin` with a tested, saturated 64-bit helper. Inverted or empty guest scissor bounds now produce a zero extent, not a huge unsigned rectangle. Leave original 1× render-target dimensions and GPU presets unchanged. Geometry tests include inverted, odd, negative, and 64-bit extreme values.

## Next native milestones (still NOT done)

- Render resolution: actual color/depth backing texture alias handling, Vulkan raster viewport/scissor scaling, copy/clear/readback transforms, upscaler/FSR and per-title options. A geometry helper alone is not resolution reduction.
- Updates/DLC: decrypted and matched CNMT; program/ROMFS BKTR overlay and AOC content registration. SHA-256 verifies copy integrity only.
- QLauncher: booting system applet with a full Horizon service/process bridge; firmware data lookup safety alone is not launcher support.
- NVDEC: implement host1x video decode command stream and surface synchronization before wrapping Android MediaCodec.
- Frame generation: temporal image history, interpolation, Vulkan presentation pacing with latency measurement. Host frame skipping is **not** frame generation.

The workflow `.github/workflows/synolsis-v7-native-stability-ci.yml` must pass C++20 geometry unit tests, Android Kotlin, native ARM64 assembly and APK upload before calling this release built. FPS changes require on-device comparison.
