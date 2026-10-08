# Plano de desenvolvimento — Skyline/Strato Lab

Critérios: (1) baseline compila, (2) logs e diagnóstico, (3) reproduzir bug, (4) patch isolado, (5) testes em aparelho, (6) só então merge.

## Checkpoint 0 — Ferramentas iniciais
- [x] Referência upstream Strato e catálogo de PRs com SHAs fixados.
- [x] CLI de clonagem e staging de PR com dry-run e `git apply --check`.
- [x] Proteções: diretório sujo, revisão inesperada e branch repetida.
- [x] Diagnóstico de GPU via ADB (PowerVR/Mali/Adreno).
- [x] Testes Python para fluxo básico (rodar localmente).
- [x] Incorporar upstream Strato em submódulo Git fixado e clonar dependências no workflow.
- [x] Publicar diffs #252/#291 e protótipo de correção #9001 (falha no carregamento de Vulkan).
- [x] Verificar compatibilidade sintática e de contexto de patches com o core pinado via GitHub Actions.
- [x] Executar testes automatizados de patches e proteções no GitHub Actions.
- [ ] APK compilado e testado em Android: build automático em andamento; não afirmar sucesso antes do artefato.
- [ ] Criar patches da nossa branch do motor após compilação confirmada.

## Checkpoint 1
- [ ] Validar PR #252, crash ao importar firmware com chaves desatualizadas.
- [ ] Validar PR #291, layout landscape em telas pequenas.
- [ ] Validar #290, exportação de saves, com backups e regressões.
- [ ] Registrar extensões Vulkan, formatos, filas e present modes; verificar fallback FIFO.
- [ ] Identificar PowerVR pelas APIs gráficas antes de aplicar otimizações específicas.

## Checkpoint 2
- [ ] Diagnóstico de criação de dispositivos Vulkan e shaders em PowerVR.
- [ ] Gamepad Android 14 e remapeamento de controles.
- [ ] Cache de shaders versionado e compilação assíncrona opt-in.
- [ ] Áudio Oboe, pausa/retomada e xrun.
- [ ] Gestão de memória/VRAM e cenários de OOM.

## Checkpoint 3 — Alto risco
- [ ] PR #298: NVDEC, dependência FFmpeg, licenças e testes.
- [ ] PR #160: VFS com atualizações/DLC em branch separada.

Não incluir arquivos de ROM, firmware ou chaves. Não instalar Turnip/AdrenoTools para PowerVR. Não adotar hacks sem benchmarks.

## Testes automáticos e acompanhamento
- [Actions](https://github.com/vps-generator/Skyline-Strato-Android/actions)
- [Build APK #1](https://github.com/vps-generator/Skyline-Strato-Android/issues/1)
- [PowerVR/Vulkan #2](https://github.com/vps-generator/Skyline-Strato-Android/issues/2)
- [Firmware/layout #3](https://github.com/vps-generator/Skyline-Strato-Android/issues/3)

Compilações no CI são experimentais, não uma release do emulador.
