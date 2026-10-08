/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2026 Synolsis Emu contributors
 */
package org.stratoemu.strato.data

import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import java.io.File
import java.io.RandomAccessFile
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.UUID

/**
 * Stores legally obtained update/DLC NSP packages in application-private
 * storage. This does NOT mount NCAs or claim they are compatible with the
 * selected game; the Strato loader must integrate CNMT title verification,
 * BKTR patching and aocsrv before these can be activated.
 *
 * Never trust an unvalidated filename, PFS0 length or external Uri.
 */
object GameContentStore {
    enum class Kind(val directory: String) {
        UPDATE("updates"), DLC("dlc")
    }

    data class Entry(val file: File, val size: Long)

    private val titlePattern = Regex("[0-9a-fA-F]{16}")
    private val safeName = Regex("[^a-zA-Z0-9._-]")

    fun isValidTitleId(titleId: String) = titlePattern.matches(titleId)

    private fun storageDir(context: Context, titleId: String, kind: Kind): File {
        require(isValidTitleId(titleId)) { "Invalid title ID" }
        val root = File(context.filesDir, "synolsis-content")
        return File(File(root, titleId.uppercase()), kind.directory)
    }

    fun list(context: Context, titleId: String, kind: Kind): List<Entry> =
        storageDir(context, titleId, kind)
            .listFiles()
            ?.filter { it.isFile && it.extension.equals("nsp", ignoreCase = true) }
            ?.sortedByDescending { it.lastModified() }
            ?.map { Entry(it, it.length()) }
            ?: emptyList()

    fun delete(context: Context, titleId: String, kind: Kind, filename: String): Boolean {
        if (filename != File(filename).name || !filename.endsWith(".nsp", true)) return false
        return File(storageDir(context, titleId, kind), filename).delete()
    }

    fun importNsp(context: Context, titleId: String, kind: Kind, source: Uri): Entry {
        val directory = storageDir(context, titleId, kind)
        check(directory.isDirectory || directory.mkdirs()) { "Cannot create content directory" }

        val displayName = context.contentResolver.query(
            source, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null
        )?.use { cursor ->
            if (cursor.moveToFirst()) cursor.getString(0) else null
        } ?: "content.nsp"
        require(displayName.lowercase().endsWith(".nsp")) { "Only .nsp packages are supported" }

        val basename = displayName.substringBeforeLast(".")
            .replace(safeName, "_").take(72).trim('.', '_').ifEmpty { "content" }
        val destination = File(directory, "${System.currentTimeMillis()}_${basename}_${UUID.randomUUID().toString().take(8)}.nsp")
        val staging = File(directory, destination.name + ".partial")
        val reserveBytes = 64L * 1024L * 1024L

        try {
            var written = 0L
            context.contentResolver.openInputStream(source)?.use { input ->
                staging.outputStream().buffered().use { output ->
                    val buffer = ByteArray(128 * 1024)
                    while (true) {
                        val count = input.read(buffer)
                        if (count < 0) break
                        if (count == 0) continue
                        written += count
                        check(staging.usableSpace > count.toLong() + reserveBytes) {
                            "Not enough free space to import NSP"
                        }
                        output.write(buffer, 0, count)
                    }
                }
            } ?: error("Cannot read selected content")
            check(written >= 16L) { "NSP file is empty or incomplete" }

            validatePfs0(staging)

            // Same-directory rename is atomic on Android's app-private storage.
            check(staging.renameTo(destination)) { "Could not finalize imported NSP" }
            return Entry(destination, written)
        } finally {
            if (staging.exists()) staging.delete()
        }
    }

    /** Validates NSP/PFS0 layout and every entry range before storing it. */
    private fun validatePfs0(file: File) {
        RandomAccessFile(file, "r").use { raf ->
            val header = ByteArray(16)
            raf.readFully(header)
            require(String(header, 0, 4, Charsets.US_ASCII) == "PFS0") {
                "Not a standard PFS0 NSP package"
            }
            val h = ByteBuffer.wrap(header).order(ByteOrder.LITTLE_ENDIAN)
            h.position(4)
            val count = h.int
            val namesSize = h.int
            require(count in 1..2048 && namesSize in 1..(8 * 1024 * 1024)) {
                "Invalid NSP entry count or string table"
            }
            val tableSize = count.toLong() * 24L
            val bodyOffset = 16L + tableSize + namesSize
            require(bodyOffset <= raf.length()) { "Truncated NSP file table" }
            val entryTable = ByteArray(tableSize.toInt())
            raf.readFully(entryTable)
            val stringTable = ByteArray(namesSize)
            raf.readFully(stringTable)
            val entries = ByteBuffer.wrap(entryTable).order(ByteOrder.LITTLE_ENDIAN)
            var ncaCount = 0
            val seenNames = HashSet<String>()
            repeat(count) {
                val offset = entries.long
                val size = entries.long
                val nameOffset = entries.int
                entries.int // reserved
                require(offset >= 0 && size >= 0 && offset <= raf.length() - bodyOffset
                    && size <= raf.length() - bodyOffset - offset) { "Invalid NSP entry bounds" }
                require(nameOffset in 0 until namesSize) { "Invalid NSP filename offset" }
                val nameEnd = (nameOffset until namesSize)
                    .firstOrNull { stringTable[it].toInt() == 0 } ?: namesSize
                // NSP/PFS0 filenames must be NUL terminated inside the
                // declared string table. A missing terminator can hide an
                // intentionally malformed metadata table.
                require(nameEnd < namesSize) { "Unterminated NSP filename" }
                val name = String(stringTable, nameOffset, nameEnd - nameOffset, Charsets.UTF_8)
                require(name.isNotBlank() && '/' !in name && '\\' !in name &&
                    name != "." && name != ".." && name.all { it.code in 0x20..0x7E }) {
                    "Invalid NSP entry filename"
                }
                require(seenNames.add(name.lowercase(java.util.Locale.ROOT))) {
                    "Duplicate NSP entry filename"
                }
                if (name.endsWith(".nca", ignoreCase = true)) ncaCount++
            }
            require(ncaCount > 0) { "NSP contains no NCA content" }
        }
    }
}
