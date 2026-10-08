/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2026 Synolsis Emu contributors
 */
package org.stratoemu.strato.settings

import androidx.preference.ListPreference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SeekBarPreference
import androidx.preference.TwoStatePreference

/**
 * Actual native-backed performance/memory presets.
 * These don't simulate extra VRAM or replace the Switch's GPU API.
 * One operation writes the existing settings that libskyline already reads.
 */
object SynolsisGpuProfiles {
    const val CONSERVATIVE = "conservative"
    const val BALANCED = "balanced"
    const val EXPERIMENTAL = "experimental"

    fun apply(settings : EmulationSettings, profile : String) : Boolean {
        when (profile) {
            CONSERVATIVE -> {
                settings.executorSlotCountScale = 3
                settings.executorFlushThreshold = 128
                settings.freeGuestTextureMemory = true
            }
            BALANCED -> {
                settings.executorSlotCountScale = 4
                settings.executorFlushThreshold = 256
                settings.freeGuestTextureMemory = true
            }
            EXPERIMENTAL -> {
                settings.executorSlotCountScale = 6
                settings.executorFlushThreshold = 512
                settings.freeGuestTextureMemory = false
            }
            else -> return false
        }
        // Keep the native shader cache enabled in every profile, and avoid
        // enabling dangerous clock/driver hacks on the 6GB Adreno test phone.
        settings.disableShaderCache = false
        return true
    }
}

/** Updates native settings and the visible preference values together. */
internal fun PreferenceFragmentCompat.wireSynolsisGpuProfiles(settings : EmulationSettings) {
    findPreference<ListPreference>("synolsis_gpu_preset")?.setOnPreferenceChangeListener { _, newValue ->
        if (!SynolsisGpuProfiles.apply(settings, newValue as? String ?: "")) {
            false
        } else {
            findPreference<SeekBarPreference>("executor_slot_count_scale")?.value =
                settings.executorSlotCountScale
            findPreference<SeekBarPreference>("executor_flush_threshold")?.value =
                settings.executorFlushThreshold
            findPreference<TwoStatePreference>("free_guest_texture_memory")?.isChecked =
                settings.freeGuestTextureMemory
            findPreference<TwoStatePreference>("disable_shader_cache")?.isChecked =
                settings.disableShaderCache
            true
        }
    }
}

/**
 * Only readback-related precision changes are exposed here.
 * This is not a floating-point shader accuracy selector or a CPU accuracy hack.
 */
internal fun PreferenceFragmentCompat.wireSynolsisReadbackPrecision(settings : EmulationSettings) {
    findPreference<ListPreference>("synolsis_readback_precision")
        ?.setOnPreferenceChangeListener { _, value ->
            when (value as? String) {
                "accurate" -> {
                    settings.enableFastGpuReadbackHack = false
                    settings.enableFastReadbackWrites = false
                }
                "balanced" -> {
                    settings.enableFastGpuReadbackHack = true
                    settings.enableFastReadbackWrites = false
                }
                "experimental" -> {
                    settings.enableFastGpuReadbackHack = true
                    settings.enableFastReadbackWrites = true
                }
                else -> return@setOnPreferenceChangeListener false
            }
            findPreference<TwoStatePreference>("enable_fast_gpu_readback_hack")?.isChecked =
                settings.enableFastGpuReadbackHack
            findPreference<TwoStatePreference>("enable_fast_readback_writes")?.isChecked =
                settings.enableFastReadbackWrites
            true
        }
}
