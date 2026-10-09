/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2026 Synolsis Emu contributors
 */
package org.stratoemu.strato

import android.app.AlertDialog
import android.net.Uri
import android.os.Bundle
import android.view.Gravity
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.stratoemu.strato.data.GameContentStore
import kotlin.math.roundToInt

/**
 * Game-specific, actual NSP package storage/management.
 *
 * Important: Do NOT display "installed and active". The current native
 * Strato NspLoader / aocsrv cannot mount updates or DLCs yet. These files
 * remain safely staged until native CNMT/BKTR/AOC integration is complete.
 */
class GameContentManagerActivity : AppCompatActivity() {
    companion object {
        const val EXTRA_TITLE_ID = "synolsis_game_content_title"
        const val EXTRA_GAME_TITLE = "synolsis_game_content_name"
        const val EXTRA_CATEGORY = "synolsis_game_content_category"
    }

    private lateinit var titleId: String
    private lateinit var kind: GameContentStore.Kind
    private lateinit var listContainer: LinearLayout
    private lateinit var importButton: Button
    private var busy = false

    private val picker = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) importPackage(uri)
    }

    private fun dp(v: Int) = (v * resources.displayMetrics.density).roundToInt()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        titleId = intent.getStringExtra(EXTRA_TITLE_ID) ?: ""
        if (!GameContentStore.isValidTitleId(titleId)) {
            Toast.makeText(this, R.string.synolsis_content_invalid_title, Toast.LENGTH_LONG).show()
            finish()
            return
        }
        kind = if (intent.getStringExtra(EXTRA_CATEGORY) == "dlc")
            GameContentStore.Kind.DLC else GameContentStore.Kind.UPDATE

        val scroll = ScrollView(this).apply { isFillViewport = true }
        val content = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(20), dp(24), dp(20), dp(30))
        }
        scroll.addView(content)
        setContentView(scroll)

        val title = TextView(this).apply {
            text = getString(
                if (kind == GameContentStore.Kind.UPDATE) R.string.synolsis_content_updates
                else R.string.synolsis_content_dlc
            )
            textSize = 26f
            setPadding(0, dp(8), 0, dp(10))
        }
        content.addView(title)
        content.addView(TextView(this).apply {
            text = (intent.getStringExtra(EXTRA_GAME_TITLE) ?: titleId) + "  [" + titleId + "]"
            textSize = 16f
            setPadding(0, 0, 0, dp(14))
        })
        content.addView(TextView(this).apply {
            text = getString(R.string.synolsis_content_not_active)
            textSize = 14f
            setPadding(dp(12), dp(12), dp(12), dp(16))
        })
        importButton = Button(this).apply {
            text = getString(R.string.synolsis_content_import)
            setOnClickListener { if (!busy) picker.launch(arrayOf("*/*")) }
        }
        content.addView(importButton)
        listContainer = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(0, dp(14), 0, 0)
        }
        content.addView(listContainer)
        refreshList()
    }

    private fun refreshList() {
        listContainer.removeAllViews()
        val entries = GameContentStore.list(this, titleId, kind)
        if (entries.isEmpty()) {
            listContainer.addView(TextView(this).apply {
                text = getString(R.string.synolsis_content_empty)
                setPadding(0, dp(20), 0, dp(20))
            })
            return
        }
        for (entry in entries) {
            val row = LinearLayout(this).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = Gravity.CENTER_VERTICAL
                setPadding(0, dp(10), 0, dp(10))
            }
            row.addView(TextView(this).apply {
                val summary = getString(R.string.synolsis_content_package_summary, entry.displayName, entry.size / 1048576.0, entry.ncaCount, entry.cnmtCount)
                text = if (entry.sha256 != null) summary + "\n" +
                    getString(R.string.synolsis_content_checksum_label, entry.sha256.take(12))
                else summary
                maxLines = 4
                layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
            })
            if (entry.sha256 != null) {
                row.addView(Button(this).apply {
                    text = getString(R.string.synolsis_content_verify)
                    setOnClickListener {
                        if (busy) return@setOnClickListener
                        busy = true
                        importButton.isEnabled = false
                        lifecycleScope.launch {
                            val result = withContext(Dispatchers.IO) {
                                runCatching { GameContentStore.verifyIntegrity(entry) }
                            }
                            busy = false
                            importButton.isEnabled = true
                            val message = when {
                                result.isFailure -> R.string.synolsis_content_error
                                result.getOrNull() == true -> R.string.synolsis_content_checksum_ok
                                else -> R.string.synolsis_content_checksum_failed
                            }
                            Toast.makeText(this@GameContentManagerActivity, message, Toast.LENGTH_LONG).show()
                        }
                    }
                })
            }
            row.addView(Button(this).apply {
                text = getString(R.string.synolsis_content_delete)
                setOnClickListener {
                    if (busy) return@setOnClickListener
                    AlertDialog.Builder(this@GameContentManagerActivity)
                        .setTitle(R.string.synolsis_content_delete_confirm)
                        .setMessage(entry.file.name)
                        .setNegativeButton(R.string.no, null)
                        .setPositiveButton(R.string.yes) { _, _ ->
                            if (GameContentStore.delete(this@GameContentManagerActivity, titleId, kind, entry.file.name)) {
                                refreshList()
                            } else {
                                Toast.makeText(this@GameContentManagerActivity, R.string.synolsis_content_error, Toast.LENGTH_LONG).show()
                            }
                        }.show()
                }
            })
            listContainer.addView(row)
        }
    }

    private fun importPackage(uri: Uri) {
        if (busy) return
        busy = true
        importButton.isEnabled = false
        importButton.text = getString(R.string.synolsis_content_importing)
        lifecycleScope.launch {
            val result = withContext(Dispatchers.IO) {
                runCatching { GameContentStore.importNsp(this@GameContentManagerActivity, titleId, kind, uri) }
            }
            busy = false
            importButton.isEnabled = true
            importButton.text = getString(R.string.synolsis_content_import)
            result.fold(
                onSuccess = {
                    Toast.makeText(
                        this@GameContentManagerActivity,
                        R.string.synolsis_content_staged_success,
                        Toast.LENGTH_LONG
                    ).show()
                    refreshList()
                },
                onFailure = { e ->
                    if (e is GameContentStore.DuplicatePackageException) {
                        Toast.makeText(this@GameContentManagerActivity,
                            R.string.synolsis_content_duplicate, Toast.LENGTH_LONG).show()
                        refreshList()
                    } else {
                        AlertDialog.Builder(this@GameContentManagerActivity)
                            .setTitle(R.string.synolsis_content_error)
                            .setMessage(e.localizedMessage ?: getString(R.string.synolsis_content_error))
                            .setPositiveButton(android.R.string.ok, null).show()
                    }
                }
            )
        }
    }
}
