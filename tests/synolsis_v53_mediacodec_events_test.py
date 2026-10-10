#!/usr/bin/env python3
"""V53 MediaCodec event regression: output buffer changes are recoverable."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
src = (root / "app/src/main/cpp/skyline/soc/host1x/classes/android_media_decoder.cpp").read_text()
assert "AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED" in src
assert "AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED" in src
assert "index = AMediaCodec_dequeueOutputBuffer(codec, &info, 0);" in src
assert src.index("AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED") < src.index("if (index < 0)\n            return false;")
assert "AMediaCodec_releaseOutputBuffer(codec, static_cast<size_t>(index), false)" in src
print("PASS V53: transient Android output-buffer event handled without fake frame")
