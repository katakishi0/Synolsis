# Synolsis V43 — user-tested V42 feedback

## Frontend and settings (implemented in source)

- Removed **individual** prod.keys and title.keys landing cards. Both original import preferences remain inside **Firmware e chaves**; key material is never bundled.
- **Dados salvos** keeps its own landing card, but now filters the content settings fragment down to its existing save import/export preference only (no firmware or keys shown on this page).
- Removed the old "recommended with V39" phrase from accurate GPU fence settings. Keep V39 storage image correction intact; synchronization option remains OFF by default.
- Game drawer: no visible side bar or tap button. Invisible **28dp touch capture** on left and right screen edges. Swiping inward 72dp opens drawer from the respective side; the panel follows finger movement while dragging. Close via backdrop or back navigation.
- Removed redundant **Optimize for this device** and **Close menu** buttons inside drawer. Auto low-memory optimization remains available in Advanced settings.
- Original Android performance overlay: independent FPS, app RSS memory, host-present mean frame time, deviation, battery thermistor, available Android RAM; style classic/advanced Citron-inspired. Advanced optionally shows recent frame-time graph min/avg/max. Data is reported as host presentation; no fake emulation speed percentage, CPU or GPU temperature.
- Legacy performance settings remain stored; per-game/global settings integration preserved.

## QLauncher experimental main fallback
- The user reported *QLauncher ExeFS mounted but no rtld*.
- Strato's ExeFS classifier accepts `main` and `main.npdm`, while `NcaLoader::LoadExeFs` previously required `rtld` unconditionally.
- Now QLauncher selection checks main and NPDM, and loader can try `main.nso` as first module only when `rtld` is genuinely absent; it will not load main twice. This removes a contradictory precheck, **not** proof HOME is playable.
- Horizon home services, other NSOs and applet lifecycle may still be missing; collect `emulation.log` after testing. Do not ask for or log prod.keys/title.keys.

## NVDEC cutscenes
- Synolsis `NvDecClass::CallMethod` previously only logged unhandled methods; VIC likewise. Native MediaCodec can consume submitted compressed access units, but no guest NVDEC SMMU memory parsing or VIC output conversion exists.
- Reference: [Citron NvdecCommon](https://github.com/citron-neo/emulator/blob/d8c1886efa33f52e5e0ba95e784a92f8f2b711a9/src/video_core/host1x/nvdec_common.h) (code GPL-2.0-or-later, **do not copy into MPL source**): method 0x80 codec and 0xC0 execute; picture info, guest bitstream offset and output surfaces follow.
- Reference: [Citron VIC](https://github.com/citron-neo/emulator/blob/d8c1886efa33f52e5e0ba95e784a92f8f2b711a9/src/video_core/host1x/vic.cpp): guest-memory YUV and RGB writes after decode.
- V43 independently adds **bounded diagnostics** for NVDEC codec selection and execute to identify which method requests reach the emulator. It does not signal fake completion and **does not yet fix cutscenes**.
- To make it functional: implement guest NVDEC register state, SMMU translated bitstream extraction, H264 parser and MediaCodec access-unit submission, correct decoded color planes, VIC image output and guest synchronization.
- Preserve NieR V39 Maxwell/Kepler storage-image fix. Compare same scene/driver on V42/V43 and keep optional fence setting OFF.

## Testing checklist

1. Portuguese menu: only one Firmware e chaves card (keys accessible inside it), a separate Saves screen, no misleading V39 label.
2. Swipe from either screen edge; no visible handle, no tap-only opening. In-game drawer has no extra Optimize/Close buttons.
3. Switch advanced performance stats on; toggle FPS, RAM, frametime, deviation, battery, Android available RAM, and graph. Verify values are shown only when enabled.
4. Test QLauncher with existing firmware. If it starts, inspect missing HOS service errors; if it fails earlier, keep log.
5. Start a cutscene and capture `NVDEC guest codec selected` and `NVDEC execute requested` lines (if present), plus VIC error lines. Firmware/keys should **never** be shared.
6. Graph correctness and native function are tested by CI; runtime graphics and playable HOME/cutscenes remain unverified until device tests.
