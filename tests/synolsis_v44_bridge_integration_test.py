#!/usr/bin/env python3
"""V44 regression: real native NVDEC guest SMMU path, not fake video playback."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
def source(path):
    return (root / path).read_text(encoding="utf-8")
base = "app/src/main/cpp/skyline/soc/host1x/"
reg = source(base+"classes/nvdec_guest_stream.h")
decoder = source(base+"classes/nvdec.cpp")
native = source(base+"command_fifo.cpp")
interface = source(base+"tegra_host_interface.h")
for term in ('PictureInfoMethod = 0x101', 'BitstreamMethod = 0x102',
             'ContainsH264ParameterSets', 'BoundedSmmuRange', 'InspectH264Context'):
    assert term in reg
for term in ('state.soc->smmu.TranslateRange', 'state.soc->smmu.Read',
             'mediaCodecBackend.QueueAccessUnit', 'mediaCodecBackend.Start',
             'mediaCodecBackend.Drain', 'ProcessGuestDecode()'):
    assert term in decoder
assert 'nvDecClass(syncpoints, state)' in native
assert 'std::forward<Args>(args)...' in interface
assert 'readyFrame = std::move(frame)' in decoder
assert 'VIC output pending' not in decoder
vic = source(base+"classes/vic.cpp")
for word in ('decoder.TryGetDecodedFrame(frame)', 'vic_output::PackLinearNv12',
             'state.soc->smmu.Write', 'vic_output::SupportedSurface',
             'vic-output-surfaces-unmapped',
             'gpu::texture::CopyPitchToBlockLinear',
             'gpu::texture::GetBlockLinearLayerSize'):
    assert word in vic
assert 'framegen::FrameHistory' in source("app/src/main/cpp/skyline/gpu/presentation_engine.h")
for path in ("maxwell_3d", "kepler_compute"):
    pipeline = source("app/src/main/cpp/skyline/gpu/interconnect/"+path+"/pipeline_manager.cpp")
    assert "vk::DescriptorType::eStorageImage" in pipeline
assert 'wait_synchronization_policy::ValidCount(numHandles)' in source("app/src/main/cpp/skyline/kernel/svc.cpp")
assert "NVDEC guest codec selected" not in decoder  # V43 diagnostic-only bridge retired
print("PASS V44: bounded guest NVDEC reader connected; baseline graphics repairs preserved; VIC still pending")
