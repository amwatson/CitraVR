// Copyright CitraVR Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.features.settings.utils

import android.net.Uri
import androidx.documentfile.provider.DocumentFile
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

    data class Definition(val setting: AbstractSetting, val restartRequired: Boolean) {
        val key = requireNotNull(setting.key)
        val section = requireNotNull(setting.section)
    }

    data class Snapshot(
        val hasTitleSettings: Boolean,
        private val globalCustomValues: Map<String, String>,
        private val titleValues: Map<String, String>,
        private val userValues: Map<String, String>
    ) {
        fun isBaseOverriddenByGlobal(key: String): Boolean =
            titleValues.containsKey(key) &&
                globalCustomValues.containsKey(key) &&
                !userValues.containsKey(key)
    }

    val definitions = listOf(
        Definition(setting = IntSetting.GRAPHICS_API, restartRequired = true),
        Definition(setting = IntSetting.RESOLUTION_FACTOR, restartRequired = true),
        Definition(setting = BooleanSetting.ASYNC_SHADERS, restartRequired = true),
        Definition(setting = BooleanSetting.NEW_3DS, restartRequired = true),
        Definition(setting = IntSetting.VR_IMMERSIVE_MODE, restartRequired = true),
        Definition(setting = IntSetting.STEREOSCOPIC_3D_DEPTH, restartRequired = false)
    )

    private val definitionsByKey = definitions.associateBy { it.key }

    fun definitionFor(key: String?): Definition? = key?.let(definitionsByKey::get)

    fun formatTitleId(titleId: Long): String = String.format(Locale.ROOT, "%016X", titleId)

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
        val globalCustomValues = definitions.mapNotNull { definition ->
            val value = settings.getSection(definition.section)
                ?.getSetting(definition.key)
                ?.valueAsString
            value?.let { definition.key to it }
        }.toMap()

        val ini = findExistingFile()?.let(::readIni)
        val titleValues = readValues(ini, titleId)
        val userValues = readValues(ini, "$titleId.user")
        val effectiveValues = definitions.associate { definition ->
            val value = titleValues[definition.key] ?: currentValues.getValue(definition.key)
            definition.key to value
        }.toMutableMap().apply {
            globalCustomValues.forEach { (key, value) -> this[key] = value }
            userValues.forEach { (key, value) -> this[key] = value }
        }

        // Settings are backed by process-wide enum values. Loading the INI into a
        // Snapshot alone leaves the menu showing (and diffing against) the global
        // values, so existing per-game values can never become its baseline.
        applyValues(effectiveValues)

        return Snapshot(
            hasTitleSettings = titleValues.isNotEmpty() || userValues.isNotEmpty(),
            globalCustomValues = globalCustomValues,
            titleValues = titleValues,
            userValues = userValues
        )
    }

    fun writeUserValues(titleId: String, values: Map<String, String>): Boolean {
        if (values.isEmpty()) return true

        return updateIni({ SettingsFile.getSettingsFile(FILE_NAME) }) { ini ->
            val section = "$titleId.user"
            values.forEach { (key, value) -> ini.put(section, key, value) }
        }
    }

    fun clearUserValues(titleId: String): Boolean = updateIni(::findExistingFile) { ini ->
        ini.remove("$titleId.user")
    }

    private fun updateIni(getFile: () -> DocumentFile?, update: (Wini) -> Unit): Boolean {
        return try {
            val file = getFile() ?: return true
            val resolver = CitraApplication.appContext.contentResolver
            val input = resolver.openInputStream(file.uri)
                ?: error("Could not open ${file.uri} for reading")
            val ini = input.use(::Wini)
            update(ini)
            val output = resolver.openOutputStream(file.uri, "wt")
                ?: error("Could not open ${file.uri} for writing")
            output.use { ini.store(it) }
            true
        } catch (e: Exception) {
            Log.error("[PerGameSettings] Failed to update $FILE_NAME.ini.vr: $e")
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

    private fun readIni(file: DocumentFile): Wini? = try {
        val input = CitraApplication.appContext.contentResolver.openInputStream(file.uri)
            ?: error("Could not open ${file.uri} for reading")
        input.use(::Wini)
    } catch (e: Exception) {
        Log.error("[PerGameSettings] Failed to read $FILE_NAME.ini.vr: $e")
        null
    }

    private fun readValues(ini: Wini?, section: String): Map<String, String> =
        definitions.mapNotNull { definition ->
            ini?.get(section, definition.key)
                ?.takeIf(String::isNotEmpty)
                ?.let { definition.key to it }
        }.toMap()
}
