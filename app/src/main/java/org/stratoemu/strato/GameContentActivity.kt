/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2026 Synolsis Emu contributors
 */
package org.stratoemu.strato

import android.net.Uri
import android.os.Bundle
import android.provider.OpenableColumns
import android.view.Gravity
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import com.google.android.material.button.MaterialButton
import com.google.android.material.card.MaterialCardView
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.util.UUID

/**
 * Per-game update and DLC import library (Android portion).
 *
 * NSPs are validated, staged and removable here. This must NOT claim that an
 * imported NSP is installed or mounted by Horizon/FS: the native Strato
 * update patching and AOC services do not yet mount external content.
 */
class GameContentActivity : AppCompatActivity() {
    companion object {
        const val EXTRA_TITLE_ID = "synolsis_content_title_id"
        const val EXTRA_TITLE = "synolsis_content_title"
        private const val TYPE_UPDATE = "updates"
        private const val TYPE_DLC = "dlc"
        private const val MAX_NSP_ENTRIES = 16_384
        private const val MAX_NSP_STRING_TABLE_BYTES = 32 * 1024 * 1024
    }

    private var contentType = TYPE_UPDATE
    private lateinit var titleId : String
    private lateinit var catalog : File
    private lateinit var listing : LinearLayout

    private fun dp(n : Int) = (n * resources.displayMetrics.density + 0.5f).toInt()

    private val chooseNsp = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri : Uri? ->
        if (uri != null) {
            // Start the copy outside the main/UI thread. A typical NSP can be
            // multiple gigabytes: never read it fully into Java heap memory.
            lifecycleScope.launch {
                val kind = contentType
                val result = withContext(Dispatchers.IO) { importFile(uri, kind) }
                AlertDialog.Builder(this@GameContentActivity)
                    .setTitle(if (result.first) R.string.synolsis_import_saved else R.string.synolsis_import_failed)
                    .setMessage(result.second)
                    .setPositiveButton(android.R.string.ok, null)
                    .show()
                refresh()
            }
        }
    }

    override fun onCreate(savedInstanceState : Bundle?) {
        super.onCreate(savedInstanceState)
        val id = intent.getStringExtra(EXTRA_TITLE_ID)?.uppercase() ?: ""
        if (!id.matches(Regex("[0-9A-F]{16}"))) {
            finish()
            return
        }
        titleId = id
        contentType = savedInstanceState?.getString("content_type") ?: TYPE_UPDATE
        catalog = File(getExternalFilesDir(null) ?: filesDir, "switch/synolsis-content/$titleId")

        val scroll = ScrollView(this)
        listing = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(18), dp(20), dp(18), dp(26))
        }
        scroll.addView(listing)
        setContentView(scroll)
        refresh()
    }

    override fun onSaveInstanceState(outState : Bundle) {
        outState.putString("content_type", contentType)
        super.onSaveInstanceState(outState)
    }

    private fun button(text : String, click : () -> Unit) : MaterialButton =
        MaterialButton(this).apply {
            this.text = text
            setOnClickListener { click() }
        }

    private fun line(text : String, size : Float = 15f) : TextView =
        TextView(this).apply {
            this.text = text
            textSize = size
            setPadding(dp(8), dp(10), dp(8), dp(10))
        }

    private fun entries() : List<File> = listOf(TYPE_UPDATE, TYPE_DLC)
        .flatMap { type ->
            File(catalog, type).listFiles()
                ?.filter { it.isFile && it.extension.equals("nsp", ignoreCase = true) }
                ?: emptyList()
        }
        .sortedBy { it.name }

    private fun refresh() {
        listing.removeAllViews()
        listing.addView(line(getString(R.string.synolsis_content_title), 27f))
        listing.addView(line(intent.getStringExtra(EXTRA_TITLE).orEmpty() + " — " + titleId))
        listing.addView(line(getString(R.string.synolsis_content_not_mounted)))
        val controls = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        controls.addView(button(getString(R.string.synolsis_import_update)) {
            contentType = TYPE_UPDATE
            chooseNsp.launch(arrayOf("application/octet-stream", "application/x-nsp", "*/*"))
        }, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        controls.addView(button(getString(R.string.synolsis_import_dlc)) {
            contentType = TYPE_DLC
            chooseNsp.launch(arrayOf("application/octet-stream", "application/x-nsp", "*/*"))
        }, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        listing.addView(controls)
        val imported = entries()
        listing.addView(line(getString(R.string.synolsis_imported_count, imported.size), 19f))
        imported.forEach { file ->
            val card = MaterialCardView(this).apply {
                radius = dp(14).toFloat()
                useCompatPadding = true
            }
            val column = LinearLayout(this).apply {
                orientation = LinearLayout.VERTICAL
                setPadding(dp(14), dp(9), dp(14), dp(9))
            }
            val category = if (file.parentFile?.name == TYPE_UPDATE)
                getString(R.string.synolsis_import_update)
            else getString(R.string.synolsis_import_dlc)
            column.addView(line("$category • " + (file.length() / (1024 * 1024)) + " MB"))
            val metadata = File(file.parentFile, file.nameWithoutExtension + ".json")
            val original = runCatching { JSONObject(metadata.readText()).optString("display_name") }
                .getOrDefault(file.name)
            column.addView(line(original))
            val delete = button(getString(R.string.synolsis_content_remove)) {
                AlertDialog.Builder(this)
                    .setMessage(getString(R.string.synolsis_content_remove_confirm))
                    .setNegativeButton(android.R.string.cancel, null)
                    .setPositiveButton(R.string.synolsis_content_remove) { _, _ ->
                        file.delete()
                        metadata.delete()
                        refresh()
                    }.show()
            }
            column.addView(delete)
            card.addView(column)
            listing.addView(card)
        }
    }

    private fun readFileName(uri : Uri) : String {
        val cursor = contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)
        cursor?.use {
            if (it.moveToFirst())
                return it.getString(0) ?: "update.nsp"
        }
        return "update.nsp"
    }

    private fun validNsp(file : File) : Boolean {
        if (file.length() < 16) return false
        // PFS0 magic + entry count + string table size + padding.
        // Detects incorrectly labelled files without consuming large archives.
        FileInputStream(file).use { stream ->
            val header = ByteArray(16)
            if (stream.read(header) != header.size) return false
            if (header[0] != 'P'.code.toByte() || header[1] != 'F'.code.toByte() ||
                header[2] != 'S'.code.toByte() || header[3] != 0.toByte()) return false
            fun le32(offset : Int) : Long =
                (0..3).fold(0L) { sum, n ->
                    sum or ((header[offset + n].toLong() and 255L) shl (n * 8))
                }
            val count = le32(4)
            val strings = le32(8)
            return count >= 1L && count <= MAX_NSP_ENTRIES.toLong() &&
                strings <= MAX_NSP_STRING_TABLE_BYTES &&
                16L + count * 0x18L + strings <= file.length()
        }
    }

    private fun importFile(uri : Uri, kind : String) : Pair<Boolean, String> {
        var incomplete : File? = null
        return try {
            val fileName = readFileName(uri)
            if (!fileName.endsWith(".nsp", ignoreCase = true))
                return false to getString(R.string.synolsis_nsp_only)
            val dir = File(catalog, kind)
            if (!dir.exists() && !dir.mkdirs())
                return false to getString(R.string.synolsis_import_failed)
            val token = UUID.randomUUID().toString()
            val pendingFile = File(dir, "$token.pending")
            incomplete = pendingFile
            val destination = File(dir, "$token.nsp")
            contentResolver.openInputStream(uri)?.use { input ->
                FileOutputStream(pendingFile).use { out ->
                    input.copyTo(out, 1024 * 1024)
                    out.fd.sync()
                }
            } ?: return false to getString(R.string.synolsis_import_failed)
            if (!validNsp(pendingFile))
                return false to getString(R.string.synolsis_nsp_invalid)
            if (!pendingFile.renameTo(destination))
                return false to getString(R.string.synolsis_import_failed)
            val meta = JSONObject()
                .put("display_name", fileName)
                .put("game_id", titleId)
                .put("kind", kind)
                .put("bytes", destination.length())
                .put("native_status", "staged_not_mounted")
            File(dir, "$token.json").writeText(meta.toString())
            true to getString(R.string.synolsis_import_saved_description)
        } catch (error : Exception) {
            false to (error.localizedMessage ?: getString(R.string.synolsis_import_failed))
        } finally {
            incomplete?.delete()
        }
    }
}
