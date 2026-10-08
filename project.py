#!/usr/bin/env python3
"""Skyline/Strato Lab: upstream bootstrap and locked PR patch staging.

Standard library only; requires Git. No firmware, decryption keys or game files.
Patch import does not imply emulator correctness. Every candidate needs Android build/tests.
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
CATALOG = json.loads((ROOT / "patch_catalog.json").read_text(encoding="utf-8"))
PATCHES = {entry["id"]: entry for entry in CATALOG["patches"]}


class ProjectError(RuntimeError):
    pass


def git(*args: str, cwd: Path | None = None, capture: bool = True) -> str:
    command = ["git", *args]
    proc = subprocess.run(command, cwd=cwd, capture_output=capture, text=True, check=False)
    if proc.returncode:
        err = (proc.stderr or proc.stdout or "erro desconhecido").strip()
        raise ProjectError(f"{' '.join(command)} falhou: {err}")
    return proc.stdout.strip() if capture else ""


def require_repo(repo: Path) -> None:
    if not repo.is_dir() or git("rev-parse", "--is-inside-work-tree", cwd=repo) != "true":
        raise ProjectError(f"Não é uma pasta Git válida: {repo}")
    if not (repo / "app" / "build.gradle").exists():
        raise ProjectError("Não parece ser a base Strato: app/build.gradle não encontrado")


def require_clean(repo: Path) -> None:
    # --untracked-files=all protects existing user files too
    if git("status", "--porcelain", "--untracked-files=all", cwd=repo):
        raise ProjectError("Há alterações não commitadas. Faça commit/stash antes de aplicar um patch.")


def init(target: Path) -> None:
    if target.exists():
        raise ProjectError(f"Destino já existe: {target}")
    target.parent.mkdir(parents=True, exist_ok=True)
    git("clone", "--recursive", CATALOG["upstream"], str(target), capture=False)
    print(f"Base Strato clonada em {target}")
    print("Confira as dependências, licenças e Android Studio antes de compilar.")


def inspect(repo: Path) -> None:
    require_repo(repo)
    print("Repositório:", repo)
    print("Branch:", git("branch", "--show-current", cwd=repo))
    print("Commit:", git("rev-parse", "HEAD", cwd=repo))
    print("Limpo:", "sim" if not git("status", "--porcelain", cwd=repo) else "não")
    print("Remotes:\n", git("remote", "-v", cwd=repo))


def list_patches() -> None:
    for patch in CATALOG["patches"]:
        print(f"#{patch['id']:<3} [{patch['risk']:<12}] {patch['name']:<27} {patch['status']}")


def apply_patch(repo: Path, pr_id: int, write: bool = False) -> None:
    if pr_id not in PATCHES:
        raise ProjectError(f"PR não catalogado: #{pr_id}")
    patch = PATCHES[pr_id]
    if patch["status"] != "candidate":
        raise ProjectError("Patch de pesquisa bloqueado: não há aprovação para aplicação")
    require_repo(repo)
    require_clean(repo)
    if git("branch", "--show-current", cwd=repo) == "":
        raise ProjectError("HEAD detached: faça checkout de uma branch antes de continuar")

    # PR refs are fetched directly from the upstream repository, not a third party fork.
    remote_ref = f"refs/pull/{pr_id}/head"
    git("fetch", "--no-tags", CATALOG["upstream"], remote_ref, cwd=repo)
    pr_sha = git("rev-parse", "FETCH_HEAD", cwd=repo)
    if pr_sha != patch["head_sha"]:
        raise ProjectError(
            f"O PR #{pr_id} foi atualizado ({pr_sha[:12]}) e difere da revisão "
            f"auditada ({patch['head_sha'][:12]}). Revise o novo diff e atualize o catálogo."
        )
    merge_base = git("merge-base", "HEAD", pr_sha, cwd=repo)
    if not merge_base:
        raise ProjectError("Não foi possível localizar ancestral comum")
    diff_bytes = subprocess.run(
        ["git", "diff", "--binary", merge_base, pr_sha, "--"],
        cwd=repo, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True
    ).stdout
    if not diff_bytes:
        raise ProjectError("Nenhuma alteração no PR")
    check = subprocess.run(["git", "apply", "--check", "-"], cwd=repo, input=diff_bytes, capture_output=True)
    if check.returncode:
        details = check.stderr.decode("utf-8", "replace").strip()
        raise ProjectError("O patch não aplica limpo na base atual. Sem alterações. " + details)
    print(f"PR #{pr_id}: diff validado contra HEAD; revisão pinada {pr_sha[:12]}")
    if not write:
        print("SIMULAÇÃO: nenhum arquivo alterado. Para aplicar, passe --write")
        return
    branch = f"fix/pr-{pr_id}-{patch['name']}"
    existing = git("branch", "--list", branch, cwd=repo)
    if existing:
        raise ProjectError(f"Branch de patch já existe: {branch}")
    git("switch", "-c", branch, cwd=repo)
    applied = subprocess.run(["git", "apply", "--index", "-"], cwd=repo, input=diff_bytes, capture_output=True)
    if applied.returncode:
        raise ProjectError("Falha ao aplicar patch: " + applied.stderr.decode("utf-8", "replace"))
    git("commit", "-m", f"Import Strato PR #{pr_id}: {patch['title']}\n\n"
        f"Original author: {patch['author']}\nSource: {patch['url']}\n"
        f"Pinned PR head: {pr_sha}\nRequires Android compile/runtime verification", cwd=repo)
    print(f"Patch aplicado na branch {branch}. Falta compilar/testar no Android.")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Projeto experimental baseado em Strato/Skyline")
    sub = parser.add_subparsers(dest="command", required=True)
    a = sub.add_parser("init", help="Clona Strato com submódulos")
    a.add_argument("--target", type=Path, default=Path("strato"))
    a = sub.add_parser("inspect", help="Mostra branch, commit e estado")
    a.add_argument("--repo", type=Path, default=Path("strato"))
    sub.add_parser("patches", help="Lista PRs catalogados")
    a = sub.add_parser("apply", help="Simula ou aplica patch de PR revisado em branch independente")
    a.add_argument("--repo", type=Path, default=Path("strato"))
    a.add_argument("--pr", type=int, required=True)
    a.add_argument("--write", action="store_true", help="Realmente aplica e cria commit")
    args = parser.parse_args(argv)
    try:
        if args.command == "init":
            init(args.target.resolve())
        elif args.command == "inspect":
            inspect(args.repo.resolve())
        elif args.command == "patches":
            list_patches()
        elif args.command == "apply":
            apply_patch(args.repo.resolve(), args.pr, args.write)
    except (ProjectError, FileNotFoundError) as exc:
        print(f"ERRO: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
