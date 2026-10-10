#!/usr/bin/env python3
"""V41 regression: shader selector removed; active game drawer and safe RAM presets."""
from pathlib import Path
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]
def load(path):
    return (root / path).read_text(encoding="utf-8")

# No obsolete GLSL/GLASM/SPIR-V menu. Keep core SPIR-V shader compilation.
pref = load("app/src/main/res/xml/emulation_preferences.xml")
ET.fromstring(pref)
assert 'app:key="synolsis_shader_mode"' not in pref
for path in (
    "app/src/main/java/org/stratoemu/strato/settings/GlobalSettingsFragment.kt",
    "app/src/main/java/org/stratoemu/strato/settings/GameSettingsFragment.kt",
    "app/src/main/java/org/stratoemu/strato/settings/SynolsisGpuProfiles.kt",
):
    assert "wireSynolsisShaderMode" not in load(path)
assert not (root / "app/src/main/res/values/synolsis_shader_modes.xml").exists()
assert not (root / "app/src/main/res/values-pt-rBR/synolsis_shader_modes.xml").exists()
assert "EmitSPIRV(" in load("app/src/main/cpp/skyline/gpu/shader_manager.cpp")

# Six native-backed presets, including memory-first and existing V40 low-end.
def entries(locale, path, key):
    el = ET.fromstring(load(f"app/src/main/res/{locale}/{path}"))
    target = next(x for x in el if x.get("name") == key)
    return [x.text for x in target]
a = entries("values", "synolsis_gpu_presets.xml", "synolsis_gpu_preset_entries")
b = entries("values", "synolsis_gpu_presets.xml", "synolsis_gpu_preset_values")
c = entries("values-pt-rBR", "synolsis_arrays.xml", "synolsis_gpu_preset_entries")
assert len(a) == len(b) == len(c) == 6
assert b[:2] == ["ultra_low_end", "low_end"]
assert 'app:key="synolsis_auto_low_end"' in pref
assert 'app:key="synolsis_memory_guard"' in pref
profile = load("app/src/main/java/org/stratoemu/strato/settings/SynolsisGpuProfiles.kt")
assert "fun chooseAutomaticProfile(totalBytes:" in profile
assert "fun applyAutomatic(settings:" in profile
assert "settings.executorSlotCountScale = 1" in profile
assert "settings.shaderCompileThreads = 1" in profile
assert "settings.freeGuestTextureMemory = true" in profile
assert "settings.enableFastGpuReadbackHack = false" in profile
assert "settings.disableShaderCache = false" in profile

# Menu is a working in-game overlay, not just a screenshot or inert button.
layout = ET.fromstring(load("app/src/main/res/layout/emu_activity.xml"))
ids = {v for node in layout.iter() for k,v in node.attrib.items() if k.endswith("id")}
for name in ("synolsis_drawer_scrim","synolsis_drawer_edge",
             "synolsis_drawer_edge_right","synolsis_drawer_panel",
             "synolsis_drawer_pause","synolsis_drawer_resume",
             "synolsis_drawer_controls","synolsis_drawer_stats",
             "synolsis_drawer_settings","synolsis_drawer_exit",
             "synolsis_performance_graph"):
    assert any(s.endswith("/"+name) for s in ids), name
activity = load("app/src/main/java/org/stratoemu/strato/EmulationActivity.kt")
for token in ("private fun setupSynolsisDrawer()", "private fun showSynolsisDrawer(fromRight:",
              "private fun hideSynolsisDrawer()", "pauseEmulator()", "resumeEmulator()",
              "synolsisDrawerSettings.setOnClickListener", "installSwipe(binding.synolsisDrawerEdgeRight, true)",
              "override fun onTrimMemory(level : Int)",
              "SynolsisGpuProfiles.applyAutomatic", "memoryGuardHandler.removeCallbacks"):
    assert token in activity, token
assert "Runtime.getRuntime().gc()" not in activity
for locale in ("values", "values-pt-rBR"):
    strings = ET.fromstring(load(f"app/src/main/res/{locale}/synolsis_v41_options.xml"))
    names = {x.get("name") for x in strings}
    assert {"synolsis_drawer_title", "synolsis_drawer_settings",
            "synolsis_memory_guard_title", "synolsis_auto_low_end_title"} <= names

# Preserve user-confirmed V39 storage-image visual repair and V35 SVC.
maxwell = load("app/src/main/cpp/skyline/gpu/interconnect/maxwell_3d/pipeline_manager.cpp")
kepler = load("app/src/main/cpp/skyline/gpu/interconnect/kepler_compute/pipeline_manager.cpp")
assert "vk::DescriptorType::eStorageImage" in maxwell
assert "vk::DescriptorType::eStorageImage" in kepler
assert "wait_synchronization_policy::ValidCount(numHandles)" in load("app/src/main/cpp/skyline/kernel/svc.cpp")
print("PASS V43 regression: swipe-only drawer, removed shader modes, native RAM profiles and V39/V35 preserved")
