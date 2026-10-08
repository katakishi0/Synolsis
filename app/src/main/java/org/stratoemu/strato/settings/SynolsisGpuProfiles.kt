/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2026 Synolsis Emu contributors
 */
package org.stratoemu.strato.settings

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
