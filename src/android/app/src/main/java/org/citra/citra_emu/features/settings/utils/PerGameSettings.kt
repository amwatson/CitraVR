// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.features.settings.utils

import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import java.io.BufferedReader
import java.io.ByteArrayInputStream
import java.io.InputStreamReader
import java.util.Locale
import org.citra.citra_emu.CitraApplication
import org.citra.citra_emu.features.settings.model.AbstractBooleanSetting
import org.citra.citra_emu.features.settings.model.AbstractIntSetting
import org.citra.citra_emu.features.settings.model.AbstractSetting
import org.citra.citra_emu.features.settings.model.BooleanSetting
import org.citra.citra_emu.features.settings.model.IntSetting
import org.citra.citra_emu.features.settings.model.Settings
import org.citra.citra_emu.utils.DirectoryInitialization.userDirectory
import org.citra.citra_emu.utils.Log
import org.ini4j.Wini

/**
 * Resolves the small set of settings supported by CitraVR's per-title compatibility file.
 *
 * Values in `[TITLE_ID]` are compatibility defaults, explicit global settings override those
 * defaults, and values in `[TITLE_ID.user]` have the highest priority.
 */
object PerGameSettings {
    private const val FILE_NAME = "per_game"

    data class Definition(
        val key: String,
        val section: String,
        val setting: AbstractSetting,
        val restartRequired: Boolean
    )

    data class Snapshot(
        val fileExists: Boolean,
        val hasTitleSettings: Boolean,
        val userValues: Map<String, String>,
        val effectiveValues: Map<String, String>,
        val globalCustomValues: Map<String, String>,
        val baseValues: Map<String, String>,
        private val titleValues: Map<String, String> = emptyMap()
    ) {
        fun isBaseOverriddenByGlobal(key: String): Boolean =
            titleValues.containsKey(key) && globalCustomValues.containsKey(key)
    }

    val definitions = listOf(
        Definition(
            key = IntSetting.GRAPHICS_API.key,
            section = Settings.SECTION_RENDERER,
            setting = IntSetting.GRAPHICS_API,
            restartRequired = true
        ),
        Definition(
            key = IntSetting.RESOLUTION_FACTOR.key,
            section = Settings.SECTION_RENDERER,
            setting = IntSetting.RESOLUTION_FACTOR,
            restartRequired = true
        ),
        Definition(
            key = BooleanSetting.ASYNC_SHADERS.key,
            section = Settings.SECTION_RENDERER,
            setting = BooleanSetting.ASYNC_SHADERS,
            restartRequired = true
        ),
        Definition(
            key = BooleanSetting.NEW_3DS.key,
            section = Settings.SECTION_SYSTEM,
            setting = BooleanSetting.NEW_3DS,
            restartRequired = true
        ),
        Definition(
            key = IntSetting.VR_IMMERSIVE_MODE.key,
            section = Settings.SECTION_VR,
            setting = IntSetting.VR_IMMERSIVE_MODE,
            restartRequired = true
        ),
        Definition(
            key = IntSetting.STEREOSCOPIC_3D_DEPTH.key,
            section = Settings.SECTION_RENDERER,
            setting = IntSetting.STEREOSCOPIC_3D_DEPTH,
            restartRequired = false
        )
    )

    private val definitionsByKey = definitions.associateBy { it.key }

    fun definitionFor(key: String?): Definition? = key?.let(definitionsByKey::get)

    fun formatTitleId(titleId: Long): String =
        String.format(Locale.ROOT, "%016X", titleId)

    fun readCurrentValues(): Map<String, String> =
        definitions.associate { it.key to it.setting.valueAsString }

    fun applyValues(values: Map<String, String>) {
        definitions.forEach { definition ->
            val value = values[definition.key] ?: return@forEach
            when (val setting = definition.setting) {
                is AbstractBooleanSetting -> setting.boolean = java.lang.Boolean.parseBoolean(value)
                is AbstractIntSetting -> setting.int = value.toIntOrNull() ?: setting.int
            }
        }
    }

    fun load(titleId: String, settings: Settings): Snapshot {
        val currentValues = readCurrentValues()
        val globalCustomValues = definitions.associateNotNull { definition ->
            val value = settings.getSection(definition.section)
                ?.getSetting(definition.key)
                ?.valueAsString
            value?.let { definition.key to it }
        }

        val file = findExistingFile()
        val fileValues = file?.let(::readIniValues) ?: emptyMap()
        val titleValues = fileValues[titleId] ?: emptyMap()
        val userValues = fileValues["$titleId.user"] ?: emptyMap()
        val baseValues = definitions.associate { definition ->
            definition.key to (titleValues[definition.key] ?: currentValues.getValue(definition.key))
        }
        val effectiveValues = baseValues.toMutableMap().apply {
            globalCustomValues.forEach { (key, value) -> this[key] = value }
            userValues.forEach { (key, value) -> this[key] = value }
        }

        // Settings are backed by process-wide enum values. Loading the INI into a
        // Snapshot alone leaves the menu showing (and diffing against) the global
        // values, so existing per-game values can never become its baseline.
        applyValues(effectiveValues)

        return Snapshot(
            fileExists = file != null,
            hasTitleSettings = titleValues.isNotEmpty() || userValues.isNotEmpty(),
            userValues = userValues,
            effectiveValues = effectiveValues,
            globalCustomValues = globalCustomValues,
            baseValues = baseValues,
            titleValues = titleValues
        )
    }

    fun writeUserValues(titleId: String, values: Map<String, String>): Boolean {
        if (values.isEmpty()) return true

        return try {
            val file = SettingsFile.getSettingsFile(FILE_NAME)
            val context = CitraApplication.appContext
            val input = context.contentResolver.openInputStream(file.uri)
            val ini = input?.use(::Wini) ?: Wini(ByteArrayInputStream(byteArrayOf()))
            val section = "$titleId.user"
            values.forEach { (key, value) -> ini.put(section, key, value) }
            val output = context.contentResolver.openOutputStream(file.uri, "wt") ?: return false
            output.use(ini::store)
            true
        } catch (e: Exception) {
            Log.error("[PerGameSettings] Error writing per_game.ini.vr: ${e.message}")
            false
        }
    }

    fun clearUserValues(titleId: String): Boolean {
        val file = findExistingFile() ?: return true
        return try {
            val context = CitraApplication.appContext
            val input = context.contentResolver.openInputStream(file.uri) ?: return false
            val ini = input.use(::Wini)
            val section = "$titleId.user"
            if (!ini.containsKey(section)) return true
            ini.remove(section)
            val output = context.contentResolver.openOutputStream(file.uri, "wt") ?: return false
            output.use(ini::store)
            true
        } catch (e: Exception) {
            Log.error("[PerGameSettings] Error clearing per-game settings: ${e.message}")
            false
        }
    }

    private fun findExistingFile(): DocumentFile? {
        val root = DocumentFile.fromTreeUri(
            CitraApplication.appContext,
            Uri.parse(userDirectory)
        ) ?: return null
        return root.findFile("config")?.findFile("$FILE_NAME.ini.vr")
    }

    private fun readIniValues(file: DocumentFile): Map<String, Map<String, String>> {
        val values = linkedMapOf<String, MutableMap<String, String>>()
        var section: MutableMap<String, String>? = null
        return try {
            val input = CitraApplication.appContext.contentResolver.openInputStream(file.uri)
                ?: return emptyMap()
            input.use { stream ->
                BufferedReader(InputStreamReader(stream)).forEachLine { line ->
                    val trimmed = line.trim()
                    if (trimmed.startsWith("[") && trimmed.endsWith("]")) {
                        section = values.getOrPut(trimmed.substring(1, trimmed.length - 1)) {
                            linkedMapOf()
                        }
                    } else if (
                        section != null &&
                        !trimmed.startsWith("#") &&
                        !trimmed.startsWith(";")
                    ) {
                        val separator = trimmed.indexOf('=')
                        if (separator > 0) {
                            val key = trimmed.substring(0, separator).trim()
                            val value = trimmed.substring(separator + 1).trim()
                            if (value.isNotEmpty()) {
                                section!![key] = value
                            }
                        }
                    }
                }
            }
            values
        } catch (e: Exception) {
            Log.error("[PerGameSettings] Error reading per_game.ini.vr: ${e.message}")
            emptyMap()
        }
    }

    private inline fun <T, K, V> Iterable<T>.associateNotNull(
        transform: (T) -> Pair<K, V>?
    ): Map<K, V> = buildMap {
        for (item in this@associateNotNull) {
            transform(item)?.let { (key, value) -> put(key, value) }
        }
    }
}
