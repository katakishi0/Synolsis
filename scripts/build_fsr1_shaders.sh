#!/usr/bin/env bash
set -euo pipefail
SOURCE="app/src/main/cpp/skyline/gpu/shaders/fsr1"
OUT="${1:-/tmp/synolsis-fsr1}"
mkdir -p "$OUT"
for pass in easu rcas; do
  if [[ "$pass" == easu ]]; then
    EASU=1; RCAS=0
  else
    EASU=0; RCAS=1
  fi
  glslangValidator -V --target-env vulkan1.1 -S comp \
    -I"$SOURCE" -DSAMPLE_SLOW_FALLBACK=1 \
    -DSAMPLE_EASU="$EASU" -DSAMPLE_RCAS="$RCAS" \
    -DSAMPLE_BILINEAR=0 \
    -o "$OUT/$pass.spv" "$SOURCE/fsr_pass.comp"
done
python3 - "$OUT" <<'PY'
import pathlib,struct,sys
directory=pathlib.Path(sys.argv[1])
for name in ("easu","rcas"):
    data=(directory/f"{name}.spv").read_bytes()
    assert len(data)>128 and len(data)%4==0
    assert struct.unpack_from("<I",data)[0]==0x07230203
    print(f"Validated {name} Vulkan SPIR-V: {len(data)} bytes")
PY
