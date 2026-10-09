#!/usr/bin/env bash
# SPDX-License-Identifier: MPL-2.0
# Build-time Vulkan SPIR-V assets; no proprietary shader blobs or device keys.
set -euo pipefail
command -v glslangValidator >/dev/null || { echo "Missing glslangValidator" >&2; exit 1; }
command -v spirv-val >/dev/null || { echo "Missing spirv-val" >&2; exit 1; }
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-"$ROOT/app/src/main/assets/shaders/synolsis"}"
mkdir -p "$OUT"
bash "$ROOT/scripts/build_fsr1_shaders.sh" "$OUT"
glslangValidator -V --target-env vulkan1.1 -S comp \
  "$ROOT/app/src/main/cpp/skyline/gpu/shaders/framegen/temporal_interpolate.comp" \
  -o "$OUT/temporal_interpolate.spv"
for name in easu rcas temporal_interpolate; do
  spirv-val --target-env vulkan1.1 "$OUT/$name.spv"
done
python3 - "$OUT" <<'PY'
from pathlib import Path
import struct
import sys
out = Path(sys.argv[1])
for name in ("easu","rcas","temporal_interpolate"):
    file = out / (name + ".spv")
    data = file.read_bytes()
    assert len(data) > 400 and len(data) % 4 == 0
    assert struct.unpack_from("<I",data)[0] == 0x07230203
    print(f"Validated packaged Vulkan shader {name}: {len(data)} bytes")
PY
