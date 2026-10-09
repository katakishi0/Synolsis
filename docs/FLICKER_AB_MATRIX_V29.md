# Synolsis V29 — controlled graphics rollback
Source fork: Synolsis V28 `ffea0dfc9c4f0f67e3af729fcf739e3b48d2282f`.
Baseline: `strato-full-core` `0cdd2b254b7ff8dcddb950c5ca5ec508f675947d`.

The following native GPU implementations are restored **byte-for-byte** from the Strato mirror:
- `presentation_engine.cpp`
- `pipeline_cache_manager.cpp`
- `interconnect/maxwell_3d/active_state.cpp`

These are restored from the Strato mirror with only signature compatibility adjustments (**no GPU algorithm changes**):
- `texture/texture.cpp`: retains an ignored `permitScaledBlit` argument to match the current header, while the original Strato copy implementation is used.
- `graphics_pipeline_assembler.cpp`: retains an ignored extra worker-count constructor parameter to match the current header; the original Strato thread-pool logic is used.

Everything else remains Synolsis V28 (including UI, Horizon/QLauncher loader, settings layout, NCA patches). This is **not** a wholly pristine Strato APK; compare separately against the `strato-full-core` baseline build. The 50%/75% output-scaler, frame-skip, presentation-mode selection and unused compute modules do **not** execute on this rollback native presentation path. Test with Frame Skip 0 and presentation 100% anyway to avoid confusion.

### Portable by default
The Kotlin setting `isDocked` and XML preference now default to `false` for **new installations** and profiles that lack a saved value. An existing installation keeps its previous setting; manually turn **Use docked mode** off before A/B testing.

### A/B matrix
A — Synolsis V28 unmodified test APK (Vulkan modified).
B — Synolsis V29 original graphics rollback (portable default on clean install).
C — Strato full-core independent baseline (original Strato renderer plus the mirror's firmware-key compatibility and vendored dependencies; no Synolsis UI).
Same driver, game version/update, scene/save, scene time, cool device, shader cache policy. Compare flicker shapes and present/game FPS separately.
First launch may perform shader compilation; compare after a cache warmup. Back up save files before uninstalling any APK.

### Interpretation
- C clean / B clean / A broken: regression probably inside Synolsis GPU modifications.
- C broken / B broken / A broken: Strato baseline/Turnip emulation issue likely (further driver A/B needed).
- C clean / B broken: outside reverted graphics modules; compare GPU settings, guest cache and interconnect.
- A/B/C vary with driver: driver-dependent issue, not proof of a particular Vulkan barrier error.

This is a diagnostic checkpoint, not a verified fix for flicker or FPS.
