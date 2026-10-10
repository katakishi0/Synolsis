#!/usr/bin/env python3
"""V43 UI / real performance stats and main-only ExeFS bootstrap regression."""
from pathlib import Path
from xml.etree import ElementTree as ET

root = Path(__file__).resolve().parents[1]
app = root / "app/src/main"
def source(relative):
    return (app / relative).read_text(encoding="utf-8")
def keys(xml):
    r=ET.fromstring(xml)
    return {e.attrib.get("name") for e in r.iter() if "name" in e.attrib}

hub=source("java/org/stratoemu/strato/SynolsisSettingsHubActivity.kt")
assert 'addCard(R.string.synolsis_v42_prod_keys' not in hub
assert 'addCard(R.string.synolsis_v42_title_keys' not in hub
assert 'openSettings(SettingsActivity.SECTION_CONTENT, "save_data_manager")' in hub
settings=source("java/org/stratoemu/strato/settings/GlobalSettingsFragment.kt")
assert 'it == "save_data_manager"' in settings and 'category.removePreference' in settings
for loc in ("values","values-pt-rBR"):
    fence=source(f"res/{loc}/synolsis_gpu_fence_antiflicker.xml")
    assert "V39" not in fence
    stats=source(f"res/{loc}/synolsis_v43_statistics.xml")
    names=keys(stats)
    assert {"synolsis_stats_style_title","synolsis_stats_fps",
            "synolsis_stats_ram","synolsis_stats_graph",
            "synolsis_stats_available_ram_label"} <= names

view=source("res/layout/emu_activity.xml")
ET.fromstring(view)
assert "synolsis_drawer_edge_right" in view
assert "synolsis_drawer_handle" not in view
assert "synolsis_drawer_auto" not in view
assert "synolsis_drawer_close\"" not in view
assert "synolsis_performance_graph" in view
appKt=source("java/org/stratoemu/strato/EmulationActivity.kt")
assert 'installSwipe(binding.synolsisDrawerEdge, false)' in appKt
assert 'installSwipe(binding.synolsisDrawerEdgeRight, true)' in appKt
assert 'synolsisDrawerHandle' not in appKt
assert 'synolsisDrawerAuto' not in appKt
assert 'synolsisDrawerClose' not in appKt
assert 'binding.synolsisPerformanceGraph.addSample(averageFrametime)' in appKt
assert 'getString(R.string.synolsis_stats_available_ram_label' in appKt
settingsSrc=source("java/org/stratoemu/strato/settings/EmulationSettings.kt")
xmlPrefs=source("res/xml/emulation_preferences.xml")
ET.fromstring(xmlPrefs)
for field, key in (
    ("synolsisStatsStyle","synolsis_stats_style"),
    ("synolsisStatsFps","synolsis_stats_fps"),
    ("synolsisStatsRam","synolsis_stats_ram"),
    ("synolsisStatsFrametime","synolsis_stats_frametime"),
    ("synolsisStatsJitter","synolsis_stats_jitter"),
    ("synolsisStatsBattery","synolsis_stats_battery"),
    ("synolsisStatsAvailRam","synolsis_stats_avail_ram"),
    ("synolsisStatsGraph","synolsis_stats_graph")
):
    assert field in settingsSrc, field
    assert 'app:key="'+key+'"' in xmlPrefs, key
graph=source("java/org/stratoemu/strato/views/SynolsisPerformanceGraphView.kt")
assert 'samples.addLast(frameTimeMs)' in graph
assert 'frameTimeMs.isFinite()' in graph
assert 'String.format' in graph

loader=source("cpp/skyline/loader/nca.cpp")
scanner=source("cpp/loader_jni.cpp")
assert 'exeFs->FileExists("main")' in loader
assert 'firstModule{hasRtld ? "rtld" : "main"}' in loader
assert 'if (firstModule == nso) continue' in loader
assert 'program.exeFs->FileExists("rtld")' in scanner  # diagnostic only, not a hard gate
assert '!program.exeFs->FileExists("rtld") ||' not in scanner
assert 'nca.exeFs->FileExists("main.npdm")' in scanner

nvdec=source("cpp/skyline/soc/host1x/classes/nvdec.cpp")
vic=source("cpp/skyline/soc/host1x/classes/vic.cpp")
assert "NVDEC registers not decoded" in nvdec
assert "VIC register not handled" in vic
print("PASS V43 UI / hidden edge swipe, per-stat overlay, QLauncher optional rtld, unimplemented NVDEC accurately marked")
