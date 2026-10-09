/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2026 Synolsis Emu contributors
 *
 * An original Material frontend inspired by Yuzu/Nyushu's card navigation.
 * It reuses existing Strato activities, not foreign licensed frontend code.
 */
package org.stratoemu.strato

import android.content.ClipData
import android.content.Intent
import android.net.Uri
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.stratoemu.strato.data.AppItem
import org.stratoemu.strato.data.AppItemTag
import org.stratoemu.strato.loader.AppEntry
import org.stratoemu.strato.loader.LoaderResult
import org.stratoemu.strato.loader.RomFormat
import android.os.Bundle
import android.view.Gravity
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.FileProvider
import java.io.File
import androidx.core.content.ContextCompat
import com.google.android.material.card.MaterialCardView
import com.google.android.material.color.MaterialColors
import org.stratoemu.strato.preference.GpuDriverActivity
import org.stratoemu.strato.settings.SettingsActivity
import kotlin.math.roundToInt

/**
 * Modern settings landing screen. The cards are navigation only;
 * all actual settings and their persistence stay in their original
 * Strato classes and SharedPreferences.
 */
class SynolsisSettingsHubActivity : AppCompatActivity() {
    private external fun validateQLauncherNca(fd: Int, keysPath: String): Boolean
    private external fun findInstalledQLauncherNca(firmwarePath: String, keysPath: String): String?

    private val qlauncherPicker = registerForActivityResult(
        ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        if (uri == null) return@registerForActivityResult

        lifecycleScope.launch {
            val valid = withContext(Dispatchers.IO) {
                runCatching {
                    contentResolver.openFileDescriptor(uri, "r")?.use { descriptor ->
                        validateQLauncherNca(
                            descriptor.fd, "${filesDir.canonicalPath}/keys/")
                    } ?: false
                }.getOrDefault(false)
            }
            if (!valid) {
                Toast.makeText(this@SynolsisSettingsHubActivity,
                    R.string.synolsis_qlauncher_bad_nca, Toast.LENGTH_LONG).show()
                return@launch
            }
            runCatching {
                contentResolver.takePersistableUriPermission(
                    uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
            }

            startQLauncher(uri)
        }
    }


    private fun startQLauncher(uri: Uri) {
        val app = AppItem(AppEntry(
            getString(R.string.synolsis_qlauncher_title),
            null, "0100000000001000", "Nintendo", null,
            RomFormat.NCA, uri, LoaderResult.Success))
        startActivity(Intent(this, EmulationActivity::class.java).apply {
            data = uri
            clipData = ClipData.newRawUri("QLauncher installed firmware NCA", uri)
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            putExtra(AppItemTag, app)
            putExtra(EmulationActivity.ReturnToMainTag, true)
        })
    }

    private fun launchInstalledQLauncher() {
        lifecycleScope.launch {
            val archiveDir = File(getPublicFilesDir(),
                "switch/nand/system/Contents/registered")
            val target = withContext(Dispatchers.IO) {
                runCatching {
                    if (!archiveDir.isDirectory) return@runCatching null
                    val name = findInstalledQLauncherNca(
                        archiveDir.canonicalPath, "${filesDir.canonicalPath}/keys/")
                        ?: return@runCatching null
                    val safeFile = File(archiveDir, name).canonicalFile
                    if (safeFile.parentFile == archiveDir.canonicalFile && safeFile.isFile)
                        safeFile else null
                }.getOrNull()
            }
            if (target == null) {
                Toast.makeText(this@SynolsisSettingsHubActivity,
                    R.string.synolsis_qlauncher_not_found, Toast.LENGTH_LONG).show()
                // Manual selection remains an explicit fallback for unusual
                // firmware layouts; it is no longer the normal boot path.
                qlauncherPicker.launch(arrayOf("*/*"))
                return@launch
            }
            runCatching {
                val uri = FileProvider.getUriForFile(this@SynolsisSettingsHubActivity,
                    "${packageName}.synolsis.firmware", target)
                startQLauncher(uri)
            }.onFailure {
                Toast.makeText(this@SynolsisSettingsHubActivity,
                    R.string.synolsis_qlauncher_open_failed, Toast.LENGTH_LONG).show()
            }
        }
    }

    private fun dp(value : Int) = (resources.displayMetrics.density * value).roundToInt()

    private fun openSettings(section : String, key : String? = null) {
        startActivity(Intent(this, SettingsActivity::class.java).apply {
            putExtra(SettingsActivity.EXTRA_SYNOLSIS_SECTION, section)
            key?.let { putExtra("synolsis_target_preference", it) }
        })
    }

    override fun onCreate(state : Bundle?) {
        super.onCreate(state)

        val scroll = ScrollView(this).apply {
            clipToPadding = false
            isFillViewport = true
        }
        val column = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(8), dp(20), dp(8), dp(32))
        }
        scroll.addView(column)
        setContentView(scroll)

        val header = TextView(this).apply {
            text = getString(R.string.synolsis_hub_title)
            textSize = 30f
            setPadding(dp(24), dp(20), dp(20), dp(22))
            setOnClickListener { finish() }
            isClickable = true
            contentDescription = getString(R.string.synolsis_hub_back)
        }
        column.addView(header)

        fun addCard(titleRes : Int, descRes : Int, iconRes : Int, click : () -> Unit) {
            val card = MaterialCardView(this).apply {
                radius = dp(18).toFloat()
                cardElevation = dp(1).toFloat()
                isClickable = true
                isFocusable = true
                setOnClickListener { click() }
                layoutParams = LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT
                ).apply { setMargins(dp(12), dp(7), dp(12), dp(7)) }
            }
            val inner = LinearLayout(this).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = Gravity.CENTER_VERTICAL
                setPadding(dp(16), dp(18), dp(16), dp(18))
            }
            val icon = ImageView(this).apply {
                setImageResource(iconRes)
                imageTintList = ContextCompat.getColorStateList(this@SynolsisSettingsHubActivity, android.R.color.white)
                layoutParams = LinearLayout.LayoutParams(dp(36), dp(36)).apply {
                    marginEnd = dp(18)
                }
                contentDescription = null // The whole card has title+description.
            }
            // Use theme's accent so the palette picker affects these cards.
            icon.setColorFilter(MaterialColors.getColor(card, com.google.android.material.R.attr.colorPrimary))
            inner.addView(icon)
            val labels = LinearLayout(this).apply {
                orientation = LinearLayout.VERTICAL
            }
            labels.addView(TextView(this).apply {
                text = getString(titleRes)
                textSize = 18f
                setTypeface(typeface, android.graphics.Typeface.BOLD)
            })
            labels.addView(TextView(this).apply {
                text = getString(descRes)
                textSize = 14f
                setPadding(0, dp(5), 0, 0)
            })
            inner.addView(labels, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
            card.addView(inner)
            column.addView(card)
        }

        addCard(R.string.synolsis_hub_advanced, R.string.synolsis_hub_advanced_desc, R.drawable.ic_settings) {
            openSettings(SettingsActivity.SECTION_ADVANCED)
        }
        addCard(R.string.synolsis_hub_gpu, R.string.synolsis_hub_gpu_desc, R.drawable.ic_settings) {
            startActivity(Intent(this, GpuDriverActivity::class.java))
        }
        addCard(R.string.synolsis_hub_controls, R.string.synolsis_hub_controls_desc, R.drawable.ic_controller) {
            // Open the existing preference list for all eight controllers,
            // instead of jumping directly to player 1.
            openSettings(SettingsActivity.SECTION_CONTROLS)
        }
        addCard(R.string.synolsis_hub_theme, R.string.synolsis_hub_theme_desc, R.drawable.ic_settings) {
            openSettings(SettingsActivity.SECTION_APPEARANCE, "synolsis_palette")
        }
        addCard(R.string.synolsis_hub_firmware, R.string.synolsis_hub_firmware_desc, R.drawable.ic_play) {
            openSettings(SettingsActivity.SECTION_CONTENT, "firmware")
        }
        addCard(R.string.synolsis_hub_data, R.string.synolsis_hub_data_desc, R.drawable.ic_search) {
            openSettings(SettingsActivity.SECTION_CONTENT, "document_provider")
        }
        addCard(R.string.synolsis_hub_about, R.string.synolsis_hub_about_desc, R.drawable.ic_settings) {
            openSettings(SettingsActivity.SECTION_ABOUT)
        }
        // Automatically find the Program NCA in the installed firmware.
        // Only use the SAF picker as fallback; Horizon services may still
        // prevent the real home menu from starting.
        addCard(R.string.synolsis_qlauncher_title,
                R.string.synolsis_qlauncher_desc, R.drawable.ic_play) {
            launchInstalledQLauncher()
        }
    }
}
