# Synolsis V57 — device results from V56 and changes

Source baseline: V56 release from branch feature/synolsis-v56-cache-vp9-diagnostics, SHA 41d7d940ba5a0cac9f765bce850ac41b39117936. Device evidence: user screenshots and emulation logs (2026-10-10).

## Reproduced
* QLauncher now passes Android NCA sharing and actually loads `qlaunch.nss` from main.nso bootstrap (ExeFS has no rtld).
* The earliest concrete HOME fatal block in the provided trace: `GetInfo: Unimplemented case ID0: 28, ID1: 0`, then guest `Break` and process termination. Nintendo Switch InfoType 28, introduced in firmware 18, means `AliasRegionExtraSize` (Switchbrew/libnx).
* V57 handles this exact SVC info ID with output zero because the existing Skyline guest memory model has no extra alias reservation beyond the reported alias region. This is not a general workaround returning zero for all unknown SVCs. Home may subsequently request other unsupported services; a new device log is needed.
* NieR:Automata game trace contains NVDEC id=0x9 and `vp9-guest-picture-header-reconstruction-not-implemented`, so VP9 cutscenes are not fixed.
* Aircraft screenshot shows visibly dark/black aircraft surfaces. Trace also shows repeated `Shader Compiler (Shader): (STUBBED) called` and `Geometry shader passthrough used with no support`, indicating incomplete shader functionality but not proving a precise causal link. The log separately records `SuboptimalKHR` presentation results and `easu.spv` optional compute module absence; neither alone proves the aircraft texture error. Do not toggle experimental shader paths or reverse V39 STORAGE_IMAGE fix without controlled A/B testing.

## Implemented
* Remove `synolsis_drawer_restart_note` TextView from in-game menu layout (without touching GPU/RAM persistence or game settings).
* Extend kernel SVC GetInfo enum to `AliasRegionExtraSize = 28`; return the guest emulated extra-reserved alias size (zero in this memory model) for sub-ID 0, otherwise invalid info value. No unqualified claim that QLauncher now fully boots.
* Preserve V56 900 MB cross-run ccache and Vulkan pipeline cache fix, plus VP9-specific unavailable warning.
* Full Android ARM64 CI and source regressions in dedicated V57 workflow.

## Device tests after published V57 APK
1. Open QLauncher, capture first new `ERROR`/`WARNING` following `qlaunch.nss` and process state. A second black screen may mean another unsupported service.
2. In NieR use same game area, exact Turnip version and driver config as V39-good/V56; compare experimental fast GPU readback OFF and subgroup shuffle OFF; cache shaders ON; do not reset cache by default.
3. If aircraft remain dark, compare screenshots against V39 under same driver to distinguish long-lived shader compiler deficiency versus a V40–V57 regression.
4. NVDEC VP9 remains expected to fail; obtain logs only if its error changes.
