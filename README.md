# Skyline / Strato Android Lab

Projeto experimental de pesquisa, correcoes e desenvolvimento de um emulador Nintendo Switch para Android **baseado no Strato/Skyline**, nao em Yuzu/Eden.

> **Status v0.1:** ferramentas de desenvolvimento e catalogo de patches. Este repositorio ainda **nao** inclui o motor inteiro do Strato, APK compilado, jogos, firmware ou chaves. Patches catalogados ainda precisam ser testados no Android.

## Preparar a base (Windows)

Requisitos: Git, Python 3.9+, Android Studio e as versoes de Android NDK/CMake exigidas pelo upstream.

```powershell
git config --global core.symlinks true
python project.py patches
python project.py init --target strato
python project.py inspect --repo strato
python project.py apply --repo strato --pr 252
# So apos analisar a simulacao, aplicar em branch separada:
python project.py apply --repo strato --pr 252 --write
```

O modo `apply` verifica a revisao do PR, exige worktree limpo e executa `git apply --check` antes de qualquer escrita. O `--write` cria uma branch e um commit isolados. **Nunca misture patches sem revisao e testes.**

Para compilar, abra `strato/` no Android Studio e consulte o [guia oficial](https://github.com/strato-emu/strato/blob/master/BUILDING.md). O clone recursivo requer acesso a rede e pode ser grande.

## Diagnostico GPU via ADB

```powershell
python tools/gpu_report.py --output gpu_report.json
```

O diagnostico e somente leitura e identifica indicios de PowerVR, Mali ou Adreno. Nao comprova suporte a extensoes Vulkan.

## Testes

```powershell
python -m unittest discover -s tests -v
```

## Arquivos

- `project.py`: clona a base e valida/aplica patches com revisoes travadas.
- `patch_catalog.json`: catalogo de PRs candidatos, com SHA, origem e risco.
- `tools/gpu_report.py`: coleta opcional de diagnostico grafico por ADB.
- `docs/RESEARCH.md`: pesquisa de forks e fontes.
- `docs/BACKLOG.md`: checkpoints e prioridades.
- `tests/test_project.py`: testes offline da ferramenta.

## Origem, licencas e limites

- Motor upstream: [strato-emu/strato](https://github.com/strato-emu/strato), com licenca GPL-3.0-or-later; preservar avisos de copyright, creditos e licencas de submodulos.
- Ferramentas autorais neste repositorio: MIT (`LICENSE-TOOLS.txt`); isso **nao** muda a licenca do Strato.
- O projeto nao redistribui ROMs, firmware, prod.keys ou outros materiais de terceiros. Use homebrew e conteudo que tenha direito de utilizar.
