# Synolsis V50 — merged video preflight, not a user APK

## Baseline and verified checkpoint

- Source baseline: V48 branch `feature/synolsis-v48-vic-rgb-output-validation`, SHA `a6f0010d2b63e1a187c8c1ad63346203dc08bd75`. V48 CI passed independently; **no APK was released**.
- V47 merged Host1x video core CI passed, and V43 is the last published user APK at the time of this checkpoint.
- Retain V35 Horizon empty-WaitSynchronization fix and **V39 storage-image graphics repair**, including the known-good Turnip fence-off baseline.

## Real V50 source changes

1. **NVDEC H.264 bitstream framing** (integrated from the independent V49 H264 research branch): validate guest access units; accept valid Annex-B or bounded four-byte length-prefixed NALs; convert genuine guest length-prefixed NALs to Annex-B before MediaCodec. Do not generate video pixels, hide decryption failures, or feed malformed data to Android.
2. **Host1x command stability**: copy up to 1 MiB of guest pushbuffer words into a short-lived owned snapshot before validating and dispatching. The class/method sequence seen during dry-run validation is byte-for-byte identical to the executed sequence even if the guest changes its original nvmap buffer afterward. Bounded to 128 buffers and 262144 words per ioctl. Snapshot costs an extra bounded CPU copy and must be benchmarked on low-memory phones.
3. **MediaCodec output negotiation**: first request explicit 8-bit semi-planar YUV420 (Android color `0x15`), supported by the existing VIC output converter; if decoder setup fails, recreate decoder with normal default color configuration. Read the *actual* output format on FORMAT_CHANGED rather than trusting the request, log dimensions/color/stride/slice height. Unsupported flexible YUV output remains rejected instead of guessing plane stride.
4. Regression tests run H264 framing, immutable gather snapshots, NVDEC context/headers, Host1x ordered batch submit, VIC YUV420 and RGB/BGRA/RGBX, and V39/V35 continuity. Android Kotlin + C++ ARM64 assembly is CI-only.

## Unfinished features — must not be reported as complete

- **Cutscenes**: full Switch NVDEC/H264 picture processing, color formats, decoder output ordering, VIC crop/tiling and GPU texture visibility, guest asynchronous syncpoints. A compiled host video bridge does **not** establish cutscene playback.
- **Internal resolution**: still pinned to native render-target resolution. Host swapchain scaling (previously caused V20 Turnip corruption) is **not** internal 0.5x/0.75x resolution. Real scaling must address render-target texture allocations, Maxwell viewport/scissor, depth/stencil, alias copies, shader sampling and CPU/GPU memory coherence together.
- **Frame generation**: no owned prior/current Vulkan image snapshots, descriptor-bound interpolation dispatch, interpolated output surface barriers, or independently paced extra presents. Shader assets/planners are not evidence of a working feature.
- **QLauncher**: NCA and main.nso entry support is not the same as Horizon HOME services and application lifecycle. Generic frontend failure may be FileProvider/URI or native boot, requiring logs.

## How to measure progress

1. CI host tests green and ARM64 Kotlin/C++ compile green (no APK publishing).
2. Inspect redacted `emulation.log` during a known cutscene for: `NVDEC execute`, `unsupported-h264-access-unit-framing`, `NVDEC Android decoded output format`, `VIC guest image not available`, and Vulkan texture errors. **Do not upload any prod.keys or title.keys.**
3. On an authorized device, compare the same NieR:Automata location, driver and settings against V39/V43; no purple/black texture regression.
4. Before shipping a resolution option, a real Vulkan render-target allocation test must show changed GPU image extent rather than an Android view scale.
5. Before shipping framegen, verify two *different* Vulkan presented image contents from source history and a measured latency/pacing budget. No frame-duplication placebo.
