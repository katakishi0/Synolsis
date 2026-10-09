# V26 diagnostic: QLauncher header scan, compact Android settings, Vulkan transfer barrier

Scope: source-only experimental branch based on V26 scanner; NOT a working HOME Menu or proven flicker fix.

- QLauncher: filter installed firmware entries by decrypting header only before decrypting the matching Program ExeFS. Basenames/8192 limit and SAF fallback retained. Firmware v22+ NSO ZBIC and Horizon services remain missing; a matched NCA may still fail to boot.
- UI: shorter en/pt-BR labels and summaries, denser Settings Hub cards, small subtitles, localized Frame Skip entries. No changes to actual GPU features.
- Vulkan: on presentation destination images, always order earlier memory reads/writes before transfer writes, including if already in TRANSFER_DST_OPTIMAL. V25 source barrier remains. This is a narrowly scoped synchronization diagnostic, not proof that Turnip flicker is solved.
- Output resolution: mark 50/75% choices **inactive** because the renderer intentionally forces 100% in V25/V26. Guest internal resolution and FSR remain unimplemented.

Validation before shipping: compile Android ARM64; verify XML resources/Android startup, NCA scan with encrypted firmware data and missing keys; compare same game/save with Frame Skip 0, shader cache ON, texture eviction OFF, FIFO 100%, and same Turnip version. Record flicker and frame times. Compare baseline A/B/A. If corruption persists inspect aliasing, layout ownership, shaders and texture lifecycle instead of claiming Vulkan barrier fixed it.
