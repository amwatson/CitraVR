package org.citra.citra_emu.vr.utils

import android.content.Context
import androidx.preference.PreferenceManager
import org.citra.citra_emu.features.settings.utils.SettingsFile
import org.citra.citra_emu.utils.Log

object VrMainActivityUtils {
    fun doVersionUpdates(applicationContext: Context) {
            // Delete V0.3.2 settings file if present
            try {
                SettingsFile.getSettingsFile(SettingsFile.FILE_NAME_CONFIG, "ini.vr0").delete()
            } catch (e : Exception) {}
    }
}