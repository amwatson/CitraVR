package org.citra.citra_emu.vr

import androidx.preference.PreferenceManager
import org.citra.citra_emu.BuildConfig
import org.citra.citra_emu.CitraApplication
import org.citra.citra_emu.R
import org.citra.citra_emu.utils.Log
import org.citra.citra_emu.vr.utils.VRUtils

class VrCitraApplication : CitraApplication() {
    private fun updateLaunchVersionPrefs() {
        val preferences = PreferenceManager.getDefaultSharedPreferences(applicationContext)
        val releaseVersionPrev : String = preferences.getString(VRUtils.PREF_RELEASE_VERSION_NAME_LAUNCH_CURRENT, "")!!
        val releaseVersionCur : String = BuildConfig.VERSION_NAME
        preferences.edit()
            .putString(VRUtils.PREF_RELEASE_VERSION_NAME_LAUNCH_PREV, releaseVersionPrev)
            .putString(VRUtils.PREF_RELEASE_VERSION_NAME_LAUNCH_CURRENT, releaseVersionCur)
            .apply()
        Log.info("${getString(R.string.app_name)} Version: \"${preferences.getString(VRUtils.PREF_RELEASE_VERSION_NAME_LAUNCH_PREV, "")}\" (prev) -> \"${preferences.getString(
            VRUtils.PREF_RELEASE_VERSION_NAME_LAUNCH_CURRENT, "")}\" (current)")
    }

    override fun onCreate() {
        super.onCreate()
        updateLaunchVersionPrefs()
    }
}