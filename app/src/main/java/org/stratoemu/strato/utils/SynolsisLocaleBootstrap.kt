/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2026 Synolsis Emu contributors
 */
package org.stratoemu.strato.utils

import android.app.Application
import android.app.LocaleManager
import android.os.Build
import androidx.appcompat.app.AppCompatDelegate
import androidx.core.os.LocaleListCompat
import java.io.File

/**
 * Fresh installs start in English regardless of device language.
 *
 * This runs once per installation. AppCompat/Android preserve the selected
 * locale afterwards, including Brazilian Portuguese or 'System default'.
 * Never override a non-empty selection on upgrades.
 */
internal object SynolsisLocaleBootstrap {
    private const val PREFS = "synolsis_locale_bootstrap"
    private const val KEY_INITIALIZED = "initial_default_language_applied"

    fun initialize(application: Application) {
        // On Android 12 and earlier, AppCompat may restore saved locales
        // only after an activity attaches. Detect an existing installation
        // before creating the bootstrap preferences so old choices are not
        // overwritten by an early empty getApplicationLocales() result.
        val preferenceDirectory = File(application.applicationInfo.dataDir, "shared_prefs")
        val existingInstall = preferenceDirectory.listFiles()?.any {
            it.isFile && it.name.endsWith(".xml") && it.name != "$PREFS.xml"
        } == true

        val preferences = application.getSharedPreferences(PREFS, Application.MODE_PRIVATE)
        if (preferences.getBoolean(KEY_INITIALIZED, false)) return

        val appCompatSelection = AppCompatDelegate.getApplicationLocales()
        val systemSelection = if (Build.VERSION.SDK_INT >= 33) {
            application.getSystemService(LocaleManager::class.java)?.applicationLocales
        } else {
            null
        }
        if (!existingInstall && appCompatSelection.isEmpty &&
            (systemSelection == null || systemSelection.isEmpty)) {
            AppCompatDelegate.setApplicationLocales(LocaleListCompat.forLanguageTags("en"))
        }

        // The marker is deliberately independent of AndroidX's locale storage:
        // switching languages later must not reset the user's preference.
        preferences.edit().putBoolean(KEY_INITIALIZED, true).apply()
    }
}
