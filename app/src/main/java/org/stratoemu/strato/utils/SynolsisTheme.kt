/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2026 Synolsis Emu contributors
 */
package org.stratoemu.strato.utils

import android.app.Activity
import org.stratoemu.strato.R
import org.stratoemu.strato.settings.AppSettings

/** Apply before Activity.onCreate; regular theme overlays affect all main UI colors. */
internal fun Activity.applySynolsisPalette() {
    when (AppSettings(applicationContext).synolsisPalette) {
        1 -> setTheme(R.style.SynolsisTheme_Midnight)
        2 -> setTheme(R.style.SynolsisTheme_Purple)
        3 -> setTheme(R.style.SynolsisTheme_Emerald)
        4 -> setTheme(R.style.SynolsisTheme_Amoled)
        5 -> setTheme(R.style.SynolsisTheme_Amber)
        // 0: keep default theme and let Material You apply when enabled.
    }
}
