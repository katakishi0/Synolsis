# Synolsis V44 — core research checkpoint (NOT a playable feature release)

Date: 2026-10-09 (user local), built on V43 `9cf762b7ff92145a391b88f1f515d7f0962cecaa`.
This **research branch deliberately does not publish an APK**. The user requested a combined device-test version only after important subsystems are genuinely implemented and verified.

## 1. NVDEC/VIC: first hard blocker, prior to codec decoding

* `app/src/main/cpp/skyline/services/nvdrv/devices/nvhost/host1x_channel.cpp`: `Host1xChannel::Submit()` validates gather spans and **increments guest syncpoints immediately**, but deliberately comments out `state.soc->host1x.channels[...].Push(gather)`. Therefore guest NVDEC/VIC method registers often **never reach** `NvDecClass::CallMethod`, regardless of its diagnostic code or MediaCodec stub.
* `app/src/main/cpp/skyline/soc/host1x/tegra_host_interface.h`: OpDone syncpoints are also submitted immediately, with a FIXME. This is incorrect timing for real hardware video workloads. Changing timing before working command/decode path can deadlock guest code.
* `app/src/main/cpp/skyline/soc/host1x/command_fifo.h`: queue stores `span<u32>`, a **borrowed pointer into guest backing**. Unconditionally restoring asynchronous Push can make it read freed/reused memory. Need owned immutable bounded gather allocations and safe lifetime, including limits against guest-controlled huge buffers.
* `command_fifo.cpp` handles only some pushbuffer opcodes; unsupported opcodes throw and kill the guest process. Need complete/controlled command decoding with error handling before enabling queued dispatch.
* `classes/nvdec.cpp` previously lacked a register state and only logged methods. V44 adds an independent **bounds-checked 0x178-word NVDEC register shadow**, recognizes 0x80 codec, 0xC0 execute, 0x101 picture-info and 0x102 bitstream offset. Pure C++20 regression test passes. **It still does not read guest compressed bitstreams, decode or write a frame**.
* `classes/android_media_decoder.cpp` already has MediaCodec input/output methods but is not connected to guest command parsing.
* `classes/vic.cpp` is entirely unimplemented; decoded H.264/VP9 frames are not converted/written to guest SMMU surfaces.
* Source mapping reference, without copying GPL code: [Citron nvdec_common.h](https://github.com/citron-neo/emulator/blob/d8c1886efa33f52e5e0ba95e784a92f8f2b711a9/src/video_core/host1x/nvdec_common.h), [Citron nvdec.cpp](https://github.com/citron-neo/emulator/blob/d8c1886efa33f52e5e0ba95e784a92f8f2b711a9/src/video_core/host1x/nvdec.cpp), [Citron vic.cpp](https://github.com/citron-neo/emulator/blob/d8c1886efa33f52e5e0ba95e784a92f8f2b711a9/src/video_core/host1x/vic.cpp). Citron/Yuzu code uses GPL-2.0-or-later; reimplement cleanly rather than copying into MPL source.

**Engineering order:** (a) owned + size-capped gather submission with tests, (b) host1x packet and THI dispatch, (c) SMMU guest address validation + mapping using `soc/smmu.h` and nvmap handles, (d) per-codec compressed access-unit construction, (e) MediaCodec output, (f) VIC color conversion/pitch and tiled address calculations, (g) correctly ordered syncpoints/fences + cutscene device test. Keep old compatibility path until end-to-end success.

## 2. Real internal resolution, not swapchain scaling

* `gpu/presentation_engine.cpp` intentionally pins `percent=100` and `canBlit=false` due to Turnip corruption. This only controls the **Android output image**, never guest shader/render resolution.
* Guest texture `GuestTexture::dimensions` and `Texture` image allocation/copy are in `gpu/texture/texture.h`, `texture.cpp`. Render target attachment state is in `gpu/interconnect/maxwell_3d/active_state.cpp` and `pipeline_state.cpp`.
* Real 0.5x / 0.75x resolution requires separate guest/host extents, scale eligibility/blacklist decisions, color/depth attachment consistency, viewport/scissor updates, shader texel-fetch coordinate scaling, sampling/copy/resolve/readback policies, and invalidation whenever texture aliases or render-target bindings change. Blindly halving Vulkan image sizes can break guest memory synchronization and recreate V20-V38 flicker.
* Reference: [Ryujinx TextureManager UpdateRenderTargetScale](https://git.axenov.dev/Museum/ryujinx/src/commit/aea7a6631caa625d0cc97c03f1f134b623101f0e/Ryujinx.Graphics.Gpu/Image/TextureManager.cs); [Ryujinx mismatch rebind bugfix](https://www.git.axenov.dev/Museum/ryujinx/src/commit/1217a8e69b9b4feadb34c2d38209d765c9542819/src/Ryujinx.Graphics.Gpu/Engine).
* Gate for implementing a user-facing slider: a sampled render-target scene must actually use fewer native render-target pixels while UI/readback and V39 storage-image shader correctness remain stable. FPS regression test at handheld + Turnip.

## 3. Frame generation and FPS

* `gpu/frame_history.h` retains **metadata only**; `gpu/frame_interpolation_planner.h` computes timestamps, not pixels.
* `gpu/presentation_engine.h` has `computePipelines` but does not dispatch synchronized interpolation between separately retained Vulkan source images.
* Synthetic frames require two live GPU frames (bounded memory), image allocation and layout transitions, interpolation compute pass, robust swapchain cadence, guest and host FPS separated, and safe fallback when a frame is missing. At ~24 FPS, GPU compute overhead may lower game throughput. Never claim true 48/60 FPS before measuring generated output and source frame rate independently.

## 4. QLauncher error location / diagnostics

* The screenshot error "Não foi possível abrir o QLauncher do firmware instalado." comes from the **`runCatching` around `FileProvider.getUriForFile` and `startQLauncher`** in `SynolsisSettingsHubActivity.kt`. This toast alone **does not prove Horizon boot was attempted**.
* V44 separates a FileProvider-path error from an Android Activity launch error, and records exception type in Android logs without logging keys. The V43 main-only NSO loading alternative may still fail on other HOME service dependencies; don't mask that after provider startup is fixed.
* Check whether runtime `getPublicFilesDir()` maps to one of `res/xml/synolsis_firmware_paths.xml` paths. Avoid broad unrestricted provider paths.

## Verification before releasing to user's device

1. CI unit-test NVDEC register bounds and address scaling (V44 job); Android Kotlin/C++ build separately required.
2. Test guest gather ownership/bounds with fuzzed malformed packets, no process termination.
3. Instrument a real cutscene and confirm command traffic reaches decoder; recover actual frames from MediaCodec, VIC writes guest output, guest syncpoints complete **after** work.
4. Confirm 0.5x/0.75x resolution actually changes color/depth render target allocations, validated on Turnip and 2 games, without V39 texture corruption.
5. Validate generated frames have distinct image content and correct presentation timestamps without lowering source FPS materially.
6. Confirm QLauncher opens and reaches HOME lifecycle without an APK/provider error; collect sanitized logs.
7. Only then publish a consolidated release for user testing. Current V44 branch is **not yet a complete fix**.
