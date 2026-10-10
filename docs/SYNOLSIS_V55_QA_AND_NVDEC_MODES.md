# Synolsis V55 — implementation and device-validation checkpoint

Base: V54 experimental commit 6647611f96b693c6d48352a73294cf82106ed7db. Preserve V35 boot / V39 STORAGE_IMAGE renderer fixes.

## Changed
- HOME/QLauncher: dedicated same-app, read-only ContentProvider for the verified installed Program NCA. The provider accepts only basename .nca under the registered firmware directory. It does not expose arbitrary user storage. The launcher checks the stream; legacy FileProvider is fallback and errors include exception category without disclosing paths.
- NVDEC: real, per-game and global Android MediaCodec selector transmitted via NativeSettings -> AndroidSettings -> DeviceState -> NVDEC -> MediaCodec.
  - 0 Auto: standard Android codec selection.
  - 1 Hardware preferred: known Android vendor codec names, then system-selected decoder.
  - 2 Software Android: only explicit Android AVC software codecs (may be missing / slow).
  - 3 Hybrid: Android default then software on configure/start failure; **does not** switch during stalled guest cutscenes or add a GPU renderer.
- Complementos: near-black minimal activity with back arrow, single-line title, only non-empty installed/staged sections, and accent-colored bottom-right Install button. Verification, deletion, title/NSP integrity checks remain.
- Portuguese labels and warnings (updates/DLC are staged only).

## Explicit limitations
- HOME may still fail in native Loader or Horizon AM/NS/applets even after provider works.
- H264 decode pipeline may still fail at Host1x syncpoint ordering, MediaCodec crop/planes, VIC guest writes or renderer texture invalidation. No claim that cutscenes are visible until physical-device test.
- Other Storm Switch switches (dynamic state, bloom, texture compression, Vulkan asynchronous execution) were **not** duplicated without native implementation and driver validation.
- Software MediaCodec mode can consume more CPU and reduce FPS; switch only for diagnosing why videos do not appear.
- Update/DLC are stored/verified, **not mounted** until native BKTR/AOC support.

## User test
1. Open QLauncher once and collect logs tagged Synolsis-QLauncher. The new provider read-check success should appear before loader errors.
2. In game settings, enable custom settings, select NVDEC Auto, repeat cutscene, then Hybrid, then Software for comparison, restart game between changes. Collect redacted NVDEC/VIC/MediaCodec logs.
3. Inspect Complementos empty screen and after a valid owned NSP; use Install select type, verify, delete.
4. Check performance/stutter regression vs V39/54, leave experimental fast readback/trim OFF.
