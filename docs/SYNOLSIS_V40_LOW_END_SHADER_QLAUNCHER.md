# Synolsis V40 — preserve V39 graphics; low-memory controls and QLauncher ExeFS repair

## Confirmed user device results

User reports **V39 fixes NieR:Automata black/magenta block corruption** (gameplay screenshots
~19–20 FPS, ~2.8 GiB as displayed by overlay), with optional V38 GPU fence
"anti-flicker" **OFF**. V38 ON/OFF had **not** fixed that symptom. The decisive
V39 change added actual Vulkan storage image descriptor writes, resource usage,
GPU-dirty tracking, conservative barriers and identity-swizzled typed image
views for Maxwell3D and KeplerCompute. Do not disable these paths to "improve FPS".

### Why earlier fixes were not pointless

- V35: Horizon SVC 0x18 accepts zero-handle waits; fixed premature game exit.
- V36: reduced hot GPFIFO DEBUG logging overhead.
- V37: fixed image copy/blit mip dimensions and transfer ordering.
- V38: optional GPU completion callbacks delayed until fence completion.
- **V39: storage image support fixed this user's visible rendering regression.**

These are independent patches in the *same cumulative build*, not subfeatures
hidden inside the V38 anti-flicker toggle.

## V40 changes

1. Rename the V38 toggle to **Accurate GPU fence synchronization**
   / **Sincronização precisa dos fences da GPU**, keep existing setting key
   `gpu_fence_anti_flicker` for compatibility with stored per-game values, and
   leave it **OFF by default**. Users may enable it only for games with
   independent GPU timing defects. It is NOT the cause of the V39 fix.
2. Remove misleading **"Conservative (6 GB RAM)"** label. Make RAM choices
   device-independent and translate them in pt-BR.
3. Add a real **Low-end** preset using existing native settings:
   handheld, two shader-compile threads, two executor slots, flush threshold
   128, release guest texture memory, shader cache enabled, safe GPU readback,
   and no forced max clocks. It does not allocate fake VRAM; users should
   compare FPS and stutter before/after. Optional (no default change).
4. Add a real **SPIR-V shader compilation selector**: "standard" vs
   "compatibility / disable subgroup shuffle". This is backed by the existing
   `ShaderManager::profile.disable_subgroup_shuffle` flag on Android Vulkan,
   and shares the old advanced switch setting so it cannot become a dummy UI.
   The compatibility mode may cause graphical issues; restart the game.
5. `GLSL` and `GLASM` alternatives in Eden are related to the OpenGL
   backend. Synolsis on Android is Vulkan-only; Vulkan takes SPIR-V modules
   (and cannot consume unconverted GLSL or NV GLASM). Implementing true
   GLSL/GLASM requires an **entire alternate OpenGL renderer or a verified
   translation/compilation path**. These are intentionally NOT advertised as
   functioning in V40.
6. QLauncher firmware scan now distinguishes missing *decryption keys*
   from malformed/missing ExeFS. The shared NCA PFS0 parser is corrected to
   bound the RegionBacking by the actual section's declared `pfs0Size`
   after `pfs0Offset` (instead of setting the region length to the **whole**
   section, potentially passing EOF). Verified with a pure C++ bounds test.
7. Preserve the installed user firmware and keys; never download/bundle them,
   and do not bypass encryption. This addresses a real firmware NCA parsing
   bug and improves diagnosis, but **QLauncher HOME services/applet lifecycle
   are still incomplete**, so running the Switch HOME Menu is **not promised**.

## Current limitations and next implementation projects

- Native internal 0.5x/0.75x game rendering **is still missing**.
  `vulkanPresentationScalePercent` only changes the host presentation
  framebuffer dimensions; it does not shrink guest render target textures.
  Yuzu/Eden implement scaling in their rasterizer/image resource management
  and shader coordinate conversions. A proper Strato implementation must
  update render target allocations, sampled attachment dimensions,
  depth/stencil, viewport/scissor, shader texture sampling coordinates and
  resolve/copy/VI paths consistently. Do not lie with a nonfunctional 0.5x
  option.
- NVDEC/VIC guest decoding is not wired to the Android MediaCodec fallback.
  CPU/GPU/None switches copied from Eden UI would be placeholders until
  actual guest video register/bitstream, VIC and fence paths exist.
- ASTC CPU vs GPU decoding choice requires separate implementations.
  Existing Strato cannot truthfully offer three working ASTC decoders.
- High-end shader optimization/GLSL/GLASM needs additional rendering
  backends. Shader accuracy currently has one real optional knob only.

## Verification

CI builds and runs `tests/nca_pfs_bounds_test.cpp`, then
`tests/synolsis_v40_integration_test.py` and full Kotlin/C++ ARM64 build.
It validates V39 storage image and V35 SVC fixes remain present.
After installing, test V40 same driver, portable mode and saved scene,
fence accuracy OFF. Compare quality/FPS with V39. For QLauncher, record
the *exact localized new message* and firmware version; a keys error is
different from incomplete ExeFS or missing Horizon services.
