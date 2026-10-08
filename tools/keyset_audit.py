#!/usr/bin/env python3
"""Offline prod.keys/title.keys sanity checker. NEVER prints key material.

Checks formatting and recognized key generations, not the cryptographic
correctness of keys. Do not share keys or upload them to GitHub.
"""
from __future__ import annotations

import argparse
import re
from pathlib import Path

LATEST_FW = "23.0.1"
LATEST_KEY_INDEX = 0x16  # HOS 23.0+ NCA generation 0x17 (index = generation - 1)
KEY_GROUPS = ("titlekek", "key_area_key_application", "key_area_key_ocean", "key_area_key_system")
INDEXED_RE = re.compile(r"^(titlekek|key_area_key_application|key_area_key_ocean|key_area_key_system|master_key)_([0-9a-fA-F]{2})$")
HEX_RE = re.compile(r"^[0-9a-fA-F]+$")


def audit_text(content: str) -> dict:
    errors: list[str] = []
    present: set[str] = set()
    recognized: dict[str, set[int]] = {group: set() for group in (*KEY_GROUPS, "master_key")}
    for n, raw in enumerate(content.splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith(("#", ";")):
            continue
        if "=" not in line:
            errors.append(f"line {n}: missing '=' delimiter")
            continue
        key, value = (piece.strip() for piece in line.split("=", 1))
        value = value.split(";", 1)[0].strip()
        if not key or not value or not HEX_RE.fullmatch(value):
            errors.append(f"line {n}: invalid key/value formatting")
            continue
        if key in present:
            errors.append(f"line {n}: duplicated name")
            continue
        present.add(key)
        m = INDEXED_RE.fullmatch(key)
        if m:
            group, suffix = m.groups()
            if len(value) != 32:
                errors.append(f"line {n}: expected 32 hexadecimal characters for {group}")
                continue
            recognized[group].add(int(suffix, 16))
        elif key == "header_key":
            if len(value) != 64:
                errors.append(f"line {n}: expected 64 hexadecimal characters for header_key")
        # Unknown names are not reported: future keysets can include extra fields.
    latest = {
        group: (LATEST_KEY_INDEX in recognized[group])
        for group in (*KEY_GROUPS, "master_key")
    }
    return {
        "firmware_reference": LATEST_FW,
        "latest_revision": f"{LATEST_KEY_INDEX:02x}",
        "keys_count": len(present),
        "recognized_generations": {
            name: [f"{n:02x}" for n in sorted(generations)]
            for name, generations in recognized.items()
        },
        "latest_revision_fields_present": latest,
        "errors": errors,
        "note": "Formatting only. Does not verify whether any key value is authentic or usable.",
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("file", type=Path, help="Local prod.keys (never upload it)")
    args = parser.parse_args()
    report = audit_text(args.file.read_text(encoding="utf-8-sig"))
    print("Keyset format report (no key values shown)")
    print("Reference firmware:", report["firmware_reference"])
    print("Expected revision:", report["latest_revision"])
    print("Key names found:", report["keys_count"])
    for group, present in report["latest_revision_fields_present"].items():
        print(f"{group}_{report['latest_revision']}: {'present' if present else 'absent'}")
    print("Formatting errors:", len(report["errors"]))
    for msg in report["errors"]:
        print("-", msg)
    print(report["note"])


if __name__ == "__main__":
    main()
