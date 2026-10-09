#!/usr/bin/env python3
"""V40 static integration regressions; separate from Android/NDK compile."""
from pathlib import Path
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]

def source(path):
    return (root / path).read_text(encoding="utf-8")

def array(res, key):
    tree = ET.fromstring(source(f"app/src/main/res/{res}"))
    for child in tree:
        if child.get("name") == key:
            return [item.text for item in child if item.tag == "item"]
    raise AssertionError(f"missing {key} in {res}")

# Feature labels must not imply a particular physical RAM amount.
presets = array("values/synolsis_gpu_presets.xml", "synolsis_gpu_preset_entries")
values = array("values/synolsis_gpu_presets.xml", "synolsis_gpu_preset_values")
pt = array("values-pt-rBR/synolsis_arrays.xml", "synolsis_gpu_preset_entries")
assert len(presets) == len(values) == len(pt) == 5
assert values[0] == "low_end"
assert all("6 GB" not in text and "6GB" not in text for text in presets + pt)

profiles = source("app/src/main/java/org/stratoemu/strato/settings/SynolsisGpuProfiles.kt")
assert 'LOW_END = "low_end"' in profiles
for token in ("settings.isDocked = false",
              "settings.executorSlotCountScale = 2",
              "settings.shaderCompileThreads = 2",
              "settings.freeGuestTextureMemory = true",
              "settings.enableFastGpuReadbackHack = false",
              "settings.disableShaderCache = false"):
    assert token in profiles, token

# Only modes wired to the real Vulkan/SPIR-V compiler are shown.
modes = array("values/synolsis_shader_modes.xml", "synolsis_shader_mode_values")
pt_modes = array("values-pt-rBR/synolsis_shader_modes.xml", "synolsis_shader_mode_entries")
assert modes == ["standard", "compat"]
assert len(pt_modes) == len(modes)
xml = source("app/src/main/res/xml/emulation_preferences.xml")
assert 'app:key="synolsis_shader_mode"' in xml
assert 'app:key="gpu_fence_anti_flicker"' in xml
assert 'app:key="synolsis_gpu_preset"' in xml
assert "GLASM" not in "\n".join(array("values/synolsis_shader_modes.xml", "synolsis_shader_mode_entries"))

for file in ("GlobalSettingsFragment.kt", "GameSettingsFragment.kt"):
    assert "wireSynolsisShaderMode(" in source(
        f"app/src/main/java/org/stratoemu/strato/settings/{file}")
assert "settings.disableSubgroupShuffle = true" in profiles
assert "settings.disableSubgroupShuffle = false" in profiles
shader = source("app/src/main/cpp/skyline/gpu/shader_manager.cpp")
assert "EmitSPIRV(" in shader
assert "disable_subgroup_shuffle = *state.settings->disableSubgroupShuffle" in shader

# Keep V39 visual fix and V35 boot fix untouched.
maxwell = source("app/src/main/cpp/skyline/gpu/interconnect/maxwell_3d/pipeline_manager.cpp")
kepler = source("app/src/main/cpp/skyline/gpu/interconnect/kepler_compute/pipeline_manager.cpp")
assert "vk::DescriptorType::eStorageImage" in maxwell
assert "vk::DescriptorType::eStorageImage" in kepler
svc = source("app/src/main/cpp/skyline/kernel/svc.cpp")
assert "wait_synchronization_policy::ValidCount(numHandles)" in svc

# Program detection must not report missing crypto keys as malformed ExeFS.
qlauncher = source("app/src/main/cpp/loader_jni.cpp")
assert "MissingProgramKeys" in qlauncher
assert "LoaderResult::MissingKeyArea" in qlauncher
assert "programKeyMissing" in qlauncher
for loc in ("values", "values-pt-rBR"):
    ET.fromstring(source(f"app/src/main/res/{loc}/synolsis_qlauncher.xml"))
    ET.fromstring(source(f"app/src/main/res/{loc}/synolsis_gpu_fence_antiflicker.xml"))
    assert "synolsis_qlauncher_program_keys_missing" in source(
        f"app/src/main/res/{loc}/synolsis_qlauncher.xml")

nca = source("app/src/main/cpp/skyline/vfs/nca.cpp")
assert "nca_bounds::GetPfsRegion" in nca
assert "std::make_shared<RegionBacking>(backing, offset, size)" in nca
assert "sectionHeader.sha256HashInfo.pfs0Size" in nca
print("PASS V40: low-memory profile, Vulkan SPIR-V modes, QLauncher key/ExeFS diagnostics, "
      "bounded PFS0 and V35/V39 retained")
