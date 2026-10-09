# Synolsis V41 — Compact in-game menu and safe low-memory behavior

User tested V40 and requested removal of its "Shader modes" selector, a
smaller UI, a left-edge slide-out game menu like Eden, safer low-end options,
automatic optimization based on device memory, cutscene/NVDEC and QLauncher repairs.

## Implemented and source-connected

- Remove V40 shader selector and obsolete strings; keep the existing
  Vulkan/SPIR-V compiler and independent advanced subgroup shuffle toggle.
  Do not advertise nonexistent GLASM/GLSL Vulkan renderers.
- Compact the Synolsis settings hub card icon/text/padding. The in-game
  drawer is a native Android overlay, not a guest Vulkan rendering change:
  a 22dp left-edge gesture capture area, narrow visible handle, 290dp
  panel, scrim, click-to-close and Back handling. Includes working
  pause/resume, on-screen controller visibility, FPS HUD, per-game settings,
  auto profile request (next game restart), exit confirmation.
- Add Ultra low-end (one executor slot, one shader-compile thread,
  small flush threshold, guest texture release enabled) and preserve Low-end.
  These preferences directly reach existing native settings. Smaller values
  can trade FPS for memory pressure; don't claim guaranteed gains.
- Optional **Automatic low-end optimization** selects Ultra low-end, Low-end,
  Conservative or Balanced using Android MemoryInfo total and available RAM
  at game start. Opt-in and per-game or global; safe read of actual RAM,
  no hardcoded 6GB device brand assumptions, shader cache stays enabled.
- Optional **Low-memory protection** polls Android memory state every 20 s
  and reacts to trim-memory notifications by preparing safe low-memory
  settings for the **next** launch. It never frees Vulkan textures still in
  flight or forces garbage collection. This is a stability aid, not a
  real-time Vulkan allocator or OOM-proof promise.
- Preserve user-confirmed V39 graphics/Maxwell/Kepler storage image fixes,
  V35 zero-handle SVC repair, V37 copy coherency, and optional accurate GPU
  fence synchronization OFF by default.

## NVDEC cutscenes — not yet completed

In Strato V40, `NvDecClass::CallMethod` still logs unhandled register
methods; `VicClass::CallMethod` still logs unhandled VIC methods.
`AndroidMediaDecoder` has a separately working Android MediaCodec backend
for submitted compressed access units, but no code converts the guest
NVDEC registers/SMMU bitstream buffers into those access units and then
copies decoded output through VIC to guest images. A UI "NVDEC GPU" toggle
cannot make cutscenes work without that end-to-end path.

Next implementation work: map host1x/NVDEC methods and guest memory,
support H.264 first, handle MediaCodec color formats and fences, then
implement VIC output/compositing. Benchmark CPU/GPU decoders after a
working reference is established. Do not fake operation completion.

## QLauncher — incomplete

V40 fixed NCA PFS0 section bounds and distinguishing missing crypto
keys from missing ExeFS. The HOME Program still requires compatible
NSO modules, kernel services, applet lifecycle and Horizon HOME services.
The existing screenshot "QLauncher NCA could not be opened" alone cannot
identify which failure remains after V40. Collect a new log/firmware
diagnostic; do not claim HOME works merely because a Program NCA was found.

## Testing

1. Run V39/V40 reference game scene on V41 with unchanged Turnip and
   GPU fence accuracy OFF to ensure graphics stay correct.
2. Swipe from the left edge. Verify pause, resume, controls, FPS, open
   game-specific settings, auto profile, close and exit confirmation.
3. Switch Auto low-end on, restart emulation, verify selected native
   settings and RAM/FPS stability. Avoid expecting mid-game resource
   deletion or immediate GPU reconfiguration.
4. QLauncher and NVDEC cutscenes are still open issues, do not claim
   verified compatibility before device evidence.
