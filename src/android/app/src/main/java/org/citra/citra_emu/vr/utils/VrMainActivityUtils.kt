package org.citra.citra_emu.vr.utils

import android.content.Context
import androidx.preference.PreferenceManager
import org.citra.citra_emu.features.settings.utils.SettingsFile
import org.citra.citra_emu.utils.Log

object VrMainActivityUtils {
    fun doVersionUpdates(applicationContext: Context) {
        val preferences = PreferenceManager.getDefaultSharedPreferences(applicationContext)
        val releaseVersionPrev : VrReleaseVersion = VrReleaseVersion(preferences.getString(VRUtils.PREF_RELEASE_VERSION_NAME_LAUNCH_PREV, "") ?: "")
        val releaseVersionCur : VrReleaseVersion = VrReleaseVersion(preferences.getString(VRUtils.PREF_RELEASE_VERSION_NAME_LAUNCH_CURRENT, "") ?: "")
        // If the previously run build was lower that v0.4.0 (note: all these builds will not have valid release versions), wipe the config. This will
        // remove previous issues like the dpad
        // Note: Do the version check whenever the current build has a valid release tag
        if (releaseVersionCur.isRealVersion() && !releaseVersionCur.hasLowerVersionThan(VrReleaseVersion.RELEASE_VERSION_0_4_0) &&  (!releaseVersionPrev.isRealVersion() || releaseVersionPrev.hasLowerVersionThan(
                VrReleaseVersion.RELEASE_VERSION_0_4_0))) {
            Log.info("New install from prev version \"v${releaseVersionCur.getMajor()}.${releaseVersionCur.getMinor()}.${releaseVersionCur.getPatch()}\" needs update. Wiping config.ini.vr0")
            // Delete V0.3.2 settings file if present
            try {
                SettingsFile.getSettingsFile(SettingsFile.FILE_NAME_CONFIG, "ini.vr0").delete()
            } catch (e : Exception) {}
        }
    }
}