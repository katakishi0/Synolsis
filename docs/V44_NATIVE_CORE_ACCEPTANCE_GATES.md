# Synolsis core — V44 engineering gates (NOT a release)

This branch is **not** a completed build. Do not publish a user-test APK or call the following features implemented until each criterion is met by code inspection, CI and physical Android/Turnip tests.

## Actual blockers confirmed from V43 source

### QLauncher
- Android screenshot shows generic "Nao foi possivel abrir o QLauncher do firmware instalado", which is emitted by `SynolsisSettingsHubActivity.launchInstalledQLauncher()` when **FileProvider URI creation or startActivity** throws, not by the native missing-rtld validator.
- V43's main.nso fallback removed the earlier ExeFS gate but did not make HOME playable.
- V44 now logs the actual exception and distinguishes file-provider URI error from activity handoff failure.
- Need correct user-owned firmware / NCA discovery, entry NSOs and Horizon HOME applet services, not just suppress errors.
- Device requirement: launch firmware-derived HOME using the installed firmware with no manual NCA picker, navigate HOME without crashes.

### Internal rendering resolution
- The current `PresentationEngine::PresentFrame` hardcodes `percent=100` and `canBlit=false`.
- The old 50/75% `presentation_scale::ChooseExtent` changes only final swapchain size, **not the guest render targets**, and caused Turnip issues.
- Actual per-game 0.5x / 0.75x / 1x etc. must consistently scale emulated graphics attachments, Vulkan render areas, viewport/scissor and texture copies/readback, respecting format/layout/crop and re-creation on mode changes. Avoid breaking compute storage image descriptors from V39.
- Test real GPU framebuffer / image dimensions and screenshots under same conditions with meaningful GPU-memory/performance difference; not just changed settings values or letterboxed output.

### Frame generation (real displayed frames)
- V43 has `framegen::FrameHistory` tracking **metadata only** and `packed_compute::PipelineSet` construction with dispatch explicitly disabled.
- Need source snapshots A/B, synchronization, descriptor writes, storage output, compute dispatch and present scheduling of **synthesized** image. Handle animation artifacts/alpha and dynamic dimension or format changes; generated frames must not inflate game simulation FPS.
- Validate with an independent displayed-frame capture/trace and actual GPU fence order; avoid presenting duplicate images as generated output.

### NVDEC cutscenes
- `NvDecClass::CallMethod` V43 recognizes codec method 0x80 and execute method 0xC0 but does not read guest bitstream.
- `AndroidMediaDecoder` can decode properly formatted access units, but no SMMU input read/codec register parser is wired; `VicClass::CallMethod` is a stub and syncpoint OpDone is prematurely triggered in TegraHostInterface.
- Correct pipeline: guest NVDEC registers -> bounded SMMU memory reads -> codec headers and compressed packets -> MediaCodec -> validated output planes -> VIC surface transforms/guest memory writes -> completion & syncpoints.
- Success requires actual game cutscenes with audio/video, repeated seeks, overlays and no hanging; unit tests for buffer bounds and resource teardown.

## Baseline
- Preserve V35 Horizon SVC wait repair and V39 Maxwell/Kepler Vulkan storage-image fix.
- Compare V39/V43/V44 same NieR scene, same Turnip, handheld, GPU fence OFF, frame skip OFF before evaluating performance changes.

## Device proof
- CI APK generation alone validates compilation, **not** runtime playback, frame interpolation, HOME, or guest render resolution on an Adreno Android handset.
- Until all four subsystems pass their functional tests, **do not announce a completed APK**.
