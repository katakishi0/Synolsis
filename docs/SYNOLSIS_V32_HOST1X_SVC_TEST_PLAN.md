# Synolsis V32 — SVC 0x11 and host1x validation

Source base: **V31 Vulkan WSI** (commit `d864e9e26368e229acb494a412892ea2617b3a9d`) plus isolated kernel/video-command robustness changes from the experimental V31 side branch. V31 WSI itself remains an independent A/B build.

## Working source changes
- Adds `svcSignalEvent` (Horizon SVC 0x11) for valid KEvent handles and returns a guest error for invalid types/handles. `svcClearEvent` now checks types and preserves invalid-state results.
- Host1x FIFO packet parser checks available words before reading method payloads for SetClass, Incr, NonIncr and Mask commands. A malformed guest packet can no longer read beyond the end of the gather span.
- Rejects invalid host1x syncpoint IDs in Tegra Host Interface and host1x channel class paths.
- Adds independent C++20 host1x size/mask tests.
- Retains V31 WSI supported-format, transform and composite alpha Vulkan surface choices, V30 Maxwell scissor fixes and portable default for fresh installs.

## Not yet functional / not verified
- **NVDEC/VIC video playback:** no guest bitstream extraction, SMMU mapping, VIC color conversion or correctly timed guest fence pipeline. The Android MediaCodec backend is independent.
- **Other HOS SVCs:** one SVC added, not full kernel emulation.
- **Nine Sols / NieR graphical fixes:** needs A/B device logs/video to confirm; no blanket compatibility claim.
- Guest internal resolution 50/75% and GPU FSR are not implemented.
- QLauncher firmware HOME services are incomplete.

Test at equal settings using original V29 GPU, V30, V31 WSI and V32; use 100% presentation, handheld mode, cache ON, frame skip zero, same Turnip driver, same scene. Measure frametime and graphical corruption, not just FPS.
