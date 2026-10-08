#!/usr/bin/env python3
"""Read-only Android GPU / Vulkan diagnostic for Strato family; requires ADB."""
from __future__ import annotations
import argparse
import json
import re
import subprocess

PROPERTIES = {
    "abi": "ro.product.cpu.abi",
    "soc": "ro.soc.model",
    "vulkan_driver": "ro.hardware.vulkan",
    "egl_driver": "ro.hardware.egl",
    "android_sdk": "ro.build.version.sdk",
}


def identify_gpu(*texts: str) -> str:
    joined = " ".join(texts).lower()
    if re.search(r"\b(powervr|imagination|rogue|bxe|ge8)\b", joined):
        return "PowerVR / Imagination"
    if re.search(r"\b(mali|panfrost|panvk|immortalis)\b", joined):
        return "ARM Mali"
    if re.search(r"\b(adreno|freedreno|turnip|qualcomm)\b", joined):
        return "Qualcomm Adreno"
    return "Desconhecida (necessária identificação manual)"


def run_adb(adb: str, *args: str) -> str:
    p = subprocess.run([adb, *args], capture_output=True, text=True, timeout=20, check=False)
    if p.returncode:
        raise RuntimeError((p.stderr or p.stdout).strip())
    return p.stdout.strip()


def collect(adb: str) -> dict:
    props = {name: run_adb(adb, "shell", "getprop", property_name)
             for name, property_name in PROPERTIES.items()}
    surface = run_adb(adb, "shell", "dumpsys", "SurfaceFlinger")
    lines = [line.strip()[:400] for line in surface.splitlines()
             if re.search(r"(?:GLES|Vulkan|GPU vendor|renderer)", line, re.I)]
    report = {"properties": props, "render_summary": lines[:12]}
    report["vendor_guess"] = identify_gpu(*props.values(), *lines)
    report["recommendation"] = (
        "Não tentar drivers Turnip/AdrenoTools em GPU PowerVR; analisar Vulkan do sistema."
        if "PowerVR" in report["vendor_guess"] else
        "Confirmar extensões Vulkan e o caminho de driver antes de trocar renderizador."
    )
    return report


def main() -> None:
    a = argparse.ArgumentParser()
    a.add_argument("--adb", default="adb", help="Executável adb")
    a.add_argument("--output", help="Salvar relatório JSON")
    args = a.parse_args()
    report = collect(args.adb)
    data = json.dumps(report, indent=2, ensure_ascii=False)
    if args.output:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(data + "\n")
        print("Salvo:", args.output)
    else:
        print(data)


if __name__ == "__main__":
    main()
