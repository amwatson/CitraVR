// Copyright Citra Emulator Project / Azahar Emulator Project / CitraVR Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.features.updatechecker

import java.net.HttpURLConnection
import java.net.URL
import kotlin.time.Duration.Companion.seconds
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.jsonObject
import org.citra.citra_emu.utils.Log

object UpdateChecker {

    /**
     * A release version: "vX.Y.Z" for stable releases, "vX.Y.Z-series.N" (e.g.
     * "v0.6.0-beta.1") for prereleases. The leading 'v' is optional. Playtest tags
     * deliberately fail to parse, which excludes them from update checking entirely
     * (see docs/RELEASING.md).
     */
    class Version(private val parts: List<Int>, private val preRelease: Pair<String, Int>?) :
        Comparable<Version> {

        val isPreRelease get() = preRelease != null

        override fun compareTo(other: Version): Int {
            for (i in 0 until maxOf(parts.size, other.parts.size)) {
                val diff = parts.getOrElse(i) { 0 } - other.parts.getOrElse(i) { 0 }
                if (diff != 0) {
                    return diff
                }
            }
            // On the same base version, a stable release outranks any prerelease.
            if ((preRelease == null) != (other.preRelease == null)) {
                return if (preRelease == null) 1 else -1
            }
            if (preRelease == null || other.preRelease == null) {
                return 0
            }
            val seriesDiff = preRelease.first.compareTo(other.preRelease.first)
            if (seriesDiff != 0) {
                return seriesDiff
            }
            return preRelease.second - other.preRelease.second
        }

        companion object {
            fun parse(version: String): Version? {
                val trimmed = version.removePrefix("v")
                val components = trimmed.substringBefore('-').split('.')
                    .map { it.toIntOrNull() ?: return null }
                val suffix = trimmed.substringAfter('-', "")
                if (suffix.isEmpty()) {
                    return Version(components, null)
                }
                val series = suffix.substringBeforeLast('.', "")
                val number = suffix.substringAfterLast('.').toIntOrNull()
                if (series.isEmpty() || number == null) {
                    return null
                }
                return Version(components, Pair(series, number))
            }
        }
    }

    private fun getResponse(urlString: String): String? {
        var connection: HttpURLConnection? = null
        try {
            val url = URL(urlString)
            connection = url.openConnection() as HttpURLConnection
            connection.requestMethod = "GET"
            connection.connectTimeout = 15.seconds.inWholeMilliseconds.toInt()
            connection.readTimeout = 15.seconds.inWholeMilliseconds.toInt()

            if (connection.responseCode == HttpURLConnection.HTTP_OK) {
                return connection.inputStream.bufferedReader().use { it.readText() }
            } else {
                Log.error(
                    "[UpdateChecker] Failed to get HTTP response with HTTP response code ${connection.responseCode}"
                )
                return null
            }
        } catch (e: Exception) {
            Log.error(
                "[UpdateChecker] Failed to get HTTP response with Kotlin exception:\n" +
                    "Type: $e\n" +
                    "Message: ${e.message}"
            )
            return null
        } finally {
            connection?.disconnect()
        }
    }

    private fun String.stripQuotes(): String? {
        if (this.first() != '"' || this.last() != '"') {
            return null
        }
        return this.drop(1).dropLast(1)
    }

    /**
     * The newest release available in the given channel, as (tag, version), or null
     * on failure or when nothing qualifies. The stable channel only considers
     * releases not flagged as prereleases on GitHub; the prerelease channel
     * considers all of them. Playtest releases never qualify in either channel
     * because their tags don't parse as versions.
     */
    fun getLatestRelease(includePrereleases: Boolean): Pair<String, Version>? {
        val response =
            getResponse("https://api.github.com/repos/amwatson/CitraVR/releases?per_page=30")

        if (response.isNullOrEmpty()) {
            return null
        }

        try {
            return Json.decodeFromString<JsonArray>(response)
                .mapNotNull { release ->
                    val fields = release.jsonObject
                    val tag =
                        fields["tag_name"].toString().stripQuotes() ?: return@mapNotNull null
                    if (!includePrereleases && fields["prerelease"].toString() == "true") {
                        return@mapNotNull null
                    }
                    Version.parse(tag)?.let { Pair(tag, it) }
                }
                .maxByOrNull { it.second }
        } catch (e: Exception) {
            Log.error("[UpdateChecker] JSON decode failed: $e")
            return null
        }
    }
}
