/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2026 Synolsis Emu contributors
 */
package org.stratoemu.strato.data

import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import java.io.File
import java.io.FileOutputStream
import java.io.RandomAccessFile
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.security.MessageDigest
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

    data class Entry(
        val file: File,
        val size: Long,
        val displayName: String = file.name,
        val ncaCount: Int = 0,
        val cnmtCount: Int = 0,
        val sha256: String? = null
    )

    class DuplicatePackageException : IllegalArgumentException("This NSP package is already imported for this game")

    private fun metadataFile(file: File) = File(file.parentFile, file.nameWithoutExtension + ".json")

    private fun loadEntry(file: File): Entry {
        val metadata = runCatching { org.json.JSONObject(metadataFile(file).readText()) }.getOrNull()
        return Entry(
            file = file,
            size = file.length(),
            displayName = metadata?.optString("name")?.takeIf { it.isNotBlank() } ?: file.name,
            ncaCount = metadata?.optInt("nca_count") ?: 0,
            cnmtCount = metadata?.optInt("cnmt_count") ?: 0,
            sha256 = metadata?.optString("sha256")?.takeIf { it.matches(Regex("[a-fA-F0-9]{64}")) }
        )
    }

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
            ?.map(::loadEntry)
            ?: emptyList()

    /**
     * Re-hashes a stored NSP without loading it into memory. For V4/V5
     * imports whose manifests contain no SHA-256, return null (unknown).
     * This verifies file integrity, NOT Switch signatures or CNMT title IDs.
     */
    fun verifyIntegrity(entry: Entry): Boolean? {
        val expected = entry.sha256 ?: return null
        val digest = MessageDigest.getInstance("SHA-256")
        entry.file.inputStream().buffered().use { input ->
            val buffer = ByteArray(128 * 1024)
            while (true) {
                val bytes = input.read(buffer)
                if (bytes < 0) break
                if (bytes > 0) digest.update(buffer, 0, bytes)
            }
        }
        return expected.equals(digest.digest().toHex(), ignoreCase = true)
    }

    private fun ByteArray.toHex(): String =
        joinToString(separator = "") { "%02x".format(it.toInt() and 0xff) }

    fun delete(context: Context, titleId: String, kind: Kind, filename: String): Boolean {
        if (filename != File(filename).name || !filename.endsWith(".nsp", true)) return false
        val file = File(storageDir(context, titleId, kind), filename)
        if (!file.isFile) return false
        val removed = file.delete()
        if (removed) metadataFile(file).delete()
        return removed
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
            val digest = MessageDigest.getInstance("SHA-256")
            context.contentResolver.openInputStream(source)?.use { input ->
                FileOutputStream(staging).use { raw ->
                    // Stream large NSP archives using a fixed-size buffer to
                    // avoid filling the 6 GB Android device's Java heap.
                    val output = raw.buffered()
                    val buffer = ByteArray(128 * 1024)
                    while (true) {
                        val count = input.read(buffer)
                        if (count < 0) break
                        if (count == 0) continue
                        written += count
                        check(staging.usableSpace > count.toLong() + reserveBytes) {
                            "Not enough free space to import NSP"
                        }
                        digest.update(buffer, 0, count)
                        output.write(buffer, 0, count)
                    }
                    output.flush()
                    raw.fd.sync()
                }
            } ?: error("Cannot read selected content")
            check(written >= 16L) { "NSP file is empty or incomplete" }

            val info = validatePfs0(staging)
            val sha256 = digest.digest().toHex()
            // Do not store multi-gigabyte copies of the same NSP for one game.
            // A legacy import without a recorded digest is left untouched.
            if (list(context, titleId, kind).any {
                    it.size == written && it.sha256?.equals(sha256, ignoreCase = true) == true
                }) {
                throw DuplicatePackageException()
            }

            // This is a verified NSP container, NOT necessarily an update/DLC
            // for this game: encrypted CNMT metadata needs native validation.
            check(staging.renameTo(destination)) { "Could not finalize imported NSP" }
            val safeName = displayName.filter { it >= ' ' }.take(160)
            val metadata = org.json.JSONObject()
                .put("name", safeName)
                .put("selected_title_id", titleId.uppercase())
                .put("selected_kind", kind.directory)
                .put("nca_count", info.first)
                .put("cnmt_count", info.second)
                .put("native_status", "staged_not_mounted")
                .put("title_id_verified", false)
                .put("sha256", sha256)
            try {
                metadataFile(destination).writeText(metadata.toString())
            } catch (error: Exception) {
                destination.delete()
                throw error
            }
            return Entry(destination, written, safeName, info.first, info.second, sha256)
        } finally {
            if (staging.exists()) staging.delete()
        }
    }

    /** Validates NSP/PFS0 layout and every entry range before storing it. */
    private fun validatePfs0(file: File): Pair<Int, Int> {
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
            var cnmtCount = 0
            val entryNames = HashSet<String>(count)
            val segments = ArrayList<Pair<Long, Long>>(count)
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
                require(nameEnd < namesSize) { "Unterminated NSP filename" }
                val name = String(stringTable, nameOffset, nameEnd - nameOffset, Charsets.UTF_8)
                require(name.isNotBlank() && '/' !in name && '\\' !in name &&
                    name != "." && name != ".." && name.all { it.code in 0x20..0x7E }) {
                    "Invalid NSP entry filename"
                }
                require(entryNames.add(name.lowercase(java.util.Locale.ROOT))) {
                    "Duplicate NSP filename"
                }
                if (size > 0L) segments.add(offset to (offset + size))
                if (name.endsWith(".nca", ignoreCase = true)) {
                    ncaCount++
                    if (name.endsWith(".cnmt.nca", ignoreCase = true)) cnmtCount++
                }
            }
            segments.sortBy { it.first }
            for (index in 1 until segments.size)
                require(segments[index].first >= segments[index - 1].second) {
                    "Overlapping NSP entry data"
                }
            require(ncaCount > 0) { "NSP contains no NCA content" }
            require(cnmtCount > 0) { "NSP is missing CNMT metadata" }
            return ncaCount to cnmtCount
        }
    }
}
