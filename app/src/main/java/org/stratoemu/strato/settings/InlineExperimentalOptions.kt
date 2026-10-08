/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2026 Skyline-Strato-Android contributors
 */
package org.stratoemu.strato.settings

import androidx.preference.PreferenceFragmentCompat
import androidx.preference.TwoStatePreference
import org.stratoemu.strato.R
import org.stratoemu.strato.utils.GpuDriverHelper

/**
 * Keeps experimental controls in the same preference screen as the rest of
 * the emulator. Every setting is backed by EmulationSettings/NativeSettings.
 * Unsafe driver-specific modes are unavailable instead of silently enabled.
 */
internal fun PreferenceFragmentCompat.configureInlineExperimentalOptions(currentGpuDriver: String) {
    if (!GpuDriverHelper.supportsForceMaxGpuClocks()) {
        findPreference<TwoStatePreference>("force_max_gpu_clocks")?.apply {
            isEnabled = false
            summary = getString(R.string.force_max_gpu_clocks_desc_unsupported)
        }
    }

    // This optimization is for Qualcomm's proprietary Adreno driver only.
    // Turnip is a custom Vulkan driver, so disallow it on the UI and in
    // NativeSettings (which enforces the setting at game launch as well).
    if (currentGpuDriver != EmulationSettings.SYSTEM_GPU_DRIVER) {
        findPreference<TwoStatePreference>("use_direct_memory_import")?.apply {
            isEnabled = false
            summary = getString(R.string.experimental_direct_import_turnip)
        }
    }
}
