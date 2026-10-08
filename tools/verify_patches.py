#!/usr/bin/env python3
"""Verify/apply bundled Strato candidate patches without a GitHub network fetch.

Default mode is read-only. Apply mode modifies only the specified upstream working
copy and never commits changes. Use a fresh checkout for every experiment.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ALLOW = {252, 291, 9001, 2200, 2201, 2202}


class PatchError(RuntimeError):
    pass


def patch_path(number: int, root: Path = ROOT) -> Path:
    if number not in ALLOW:
        raise PatchError(f"PR #{number} not approved for local patch staging")
    path = root / "patches" / f"pr-{number}.patch"
    if not path.is_file():
        raise PatchError(f"Patch file missing: {path}")
    return path


def changed_files(patch: bytes) -> list[str]:
    """Allow only normal tracked source diffs, rejecting path traversal and unsafe modes."""
    text = patch.decode("utf-8")
    if "GIT binary patch" in text or "literal " in text:
        raise PatchError("Binary patches are not supported")
    paths = []
    for line in text.splitlines():
        if not line.startswith("diff --git "):
            continue
        match = re.fullmatch(r"diff --git a/(.+) b/(.+)", line)
        if not match or match.group(1) != match.group(2):
            raise PatchError("Renames and unexpected diff headers are not allowed")
        candidate = match.group(1)
        part = Path(candidate)
        if part.is_absolute() or not part.parts or ".." in part.parts or not candidate.startswith("app/"):
            raise PatchError("Patch would write outside upstream app/: " + candidate)
        paths.append(candidate)
    if not paths:
        raise PatchError("No source file changes found")
    if len(paths) != len(set(paths)):
        raise PatchError("Duplicate modified paths detected")
    return paths


def verify(upstream: Path, number: int, *, write: bool = False, root: Path = ROOT) -> dict:
    patch = patch_path(number, root).read_bytes()
    paths = changed_files(patch)
    upstream = upstream.resolve(strict=True)
    result = subprocess.run(["git", "rev-parse", "--show-toplevel"], cwd=upstream,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if result.returncode or Path(result.stdout.strip()).resolve() != upstream:
        raise PatchError("Upstream directory must be a Git worktree root")
    if not (upstream / "app" / "build.gradle").is_file():
        raise PatchError("Not a Strato source checkout: app/build.gradle missing")
    dirty = subprocess.run(["git", "status", "--porcelain", "--untracked-files=all"],
                           cwd=upstream, capture_output=True, text=True, check=True)
    if dirty.stdout.strip():
        raise PatchError("Upstream worktree is not clean; refusing to touch user changes")
    check = subprocess.run(["git", "apply", "--check", "-"], cwd=upstream,
                           input=patch, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if check.returncode:
        raise PatchError("Patch incompatible with this revision: " +
                         check.stderr.decode("utf-8", "replace").strip())
    if write:
        apply = subprocess.run(["git", "apply", "-"], cwd=upstream, input=patch,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if apply.returncode:
            raise PatchError("Apply failed: " + apply.stderr.decode("utf-8", "replace").strip())
    return {"pr": number, "files": paths, "applied": write}


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Check or apply bundled Strato patches")
    ap.add_argument("--upstream", type=Path, default=ROOT / "upstream/strato")
    ap.add_argument("--pr", type=int, required=True, choices=sorted(ALLOW))
    ap.add_argument("--write", action="store_true", help="Actually change worktree (requires clean checkout)")
    args = ap.parse_args(argv)
    try:
        data = verify(args.upstream, args.pr, write=args.write)
        mode = "APPLIED" if data["applied"] else "CHECK PASSED (dry run)"
        print(f"PR #{data['pr']}: {mode}; {len(data['files'])} files")
        for path in data["files"]:
            print(" -", path)
        return 0
    except (PatchError, OSError, subprocess.CalledProcessError) as error:
        print("ERROR:", error, file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
