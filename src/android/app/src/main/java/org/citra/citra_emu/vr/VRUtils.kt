package org.citra.citra_emu.vr

import android.view.KeyEvent
import org.citra.citra_emu.NativeLibrary

object VRUtils {
    val hMDType: Int
        external get
    val defaultResolutionFactor: Int
        external get

    // NOTE: keep this in-sync with HMDType in vr_settings.h
    enum class HMDType(val value: Int) {
        UNKNOWN(0),
        QUEST1(1),
        QUEST2(2),
        QUEST3(3),
        QUESTPRO(4)

    }

    // Not really VR-related, but for some reason, Citra doesn't have default mappings for gamepad
    enum class ButtonType(val nativeLibrary: Int, val android: Int) {
        BUTTON_A(NativeLibrary.ButtonType.BUTTON_A, KeyEvent.KEYCODE_BUTTON_A),
        BUTTON_B(NativeLibrary.ButtonType.BUTTON_B, KeyEvent.KEYCODE_BUTTON_B),
        BUTTON_X(NativeLibrary.ButtonType.BUTTON_X, KeyEvent.KEYCODE_BUTTON_X),
        BUTTON_Y(NativeLibrary.ButtonType.BUTTON_Y, KeyEvent.KEYCODE_BUTTON_Y),
        BUTTON_START(NativeLibrary.ButtonType.BUTTON_START, KeyEvent.KEYCODE_BUTTON_START),
        BUTTON_SELECT(NativeLibrary.ButtonType.BUTTON_SELECT, KeyEvent.KEYCODE_BUTTON_SELECT),
        BUTTON_HOME(NativeLibrary.ButtonType.BUTTON_HOME, KeyEvent.KEYCODE_BUTTON_MODE),
        BUTTON_ZL(NativeLibrary.ButtonType.BUTTON_ZL, KeyEvent.KEYCODE_BUTTON_L2),
        BUTTON_ZR(NativeLibrary.ButtonType.BUTTON_ZR, KeyEvent.KEYCODE_BUTTON_R2),
        DPAD_UP(NativeLibrary.ButtonType.DPAD_UP, KeyEvent.KEYCODE_DPAD_UP),
        DPAD_DOWN(NativeLibrary.ButtonType.DPAD_DOWN, KeyEvent.KEYCODE_DPAD_DOWN),
        DPAD_LEFT(NativeLibrary.ButtonType.DPAD_LEFT, KeyEvent.KEYCODE_DPAD_LEFT),
        DPAD_RIGHT(NativeLibrary.ButtonType.DPAD_RIGHT, KeyEvent.KEYCODE_DPAD_RIGHT),
        TRIGGER_L(NativeLibrary.ButtonType.TRIGGER_L, KeyEvent.KEYCODE_BUTTON_L1),
        TRIGGER_R(NativeLibrary.ButtonType.TRIGGER_R, KeyEvent.KEYCODE_BUTTON_R1);
        // This companion object will hold the mapping from Android to Native Library
        companion object {
            // Initialize the map once and use it for lookups
            val androidToNativeLibraryMap: Map<Int, Int> = values().associate { it.android to it.nativeLibrary }

            // Function to get the Native Library value from an Android Key Code
            inline fun androidToNativeLibrary(androidKeyCode: Int): Int? = androidToNativeLibraryMap[androidKeyCode]
        }
    }

  @JvmStatic
  fun getDefaultAxisMapping(androidAxis: Int): Int {
    return when (androidAxis) {
      14 -> NativeLibrary.ButtonType.STICK_C
      11 -> NativeLibrary.ButtonType.STICK_C
      1 -> NativeLibrary.ButtonType.STICK_LEFT
      0 -> NativeLibrary.ButtonType.STICK_LEFT
      else -> -1
    }
  }

  @JvmStatic
  fun getDefaultOrientationMapping(androidAxis: Int): Int {
    return when (androidAxis) {
      14 -> 1
      11 -> 0
      1 -> 1
      0 -> 0
      else -> -1
    }
  }

    const val PREF_RELEASE_VERSION_NAME_LAUNCH_CURRENT = "VR_ReleaseVersionName_LaunchCurrent"
    const val PREF_RELEASE_VERSION_NAME_LAUNCH_PREV = "VR_ReleaseVersionName_LaunchPrev"

    // release versions are in the form "v\d+\.\d+\.\d+". All other values are build versions
    fun isReleaseVersion(version: String): Boolean {
        return version.startsWith("v")
    }
    fun getVersionMajor(version: String): Int {
        return if (isReleaseVersion(version)) version.split(".")[0].removePrefix("v").toInt() else -1
    }
    fun getVersionMinor(version: String): Int {
        return if (isReleaseVersion(version)) version.split(".")[1].toInt() else -1
    }
    fun getVersionPatch(version: String): Int {
        return if (isReleaseVersion(version)) version.split(".")[2].toInt() else -1
    }

    fun hasLowerVersionThan(versionOrig: String, versionComp: String): Boolean {
        val majorOrig = getVersionMajor(versionOrig)
        val majorComp = getVersionMajor(versionComp)
        if (majorOrig < majorComp) return true
        if (majorOrig > majorComp) return false

        val minorOrig = getVersionMinor(versionOrig)
        val minorComp = getVersionMinor(versionComp)
        if (minorOrig < minorComp) return true
        if (minorOrig > minorComp) return false

        val patchOrig = getVersionPatch(versionOrig)
        val patchComp = getVersionPatch(versionComp)
        if (patchOrig < patchComp) return true
        if (patchOrig > patchComp) return false

        return false
    }

    fun createVersionString(major: Int, minor: Int, patch: Int): String {
        return "v$major.$minor.$patch"
    }
}
