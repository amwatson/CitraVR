// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.utils

import androidx.preference.PreferenceManager
import org.citra.citra_emu.CitraApplication
import org.citra.citra_emu.NativeLibrary
import org.citra.citra_emu.features.settings.model.Settings

/**
 * Applies the "Swap B and Y" controls option at input-dispatch time, so it
 * covers every physical input source (gamepads and VR touch controllers)
 * without rewriting the user's saved button bindings.
 */
object ButtonSwapUtils {
    private val preferences
        get() = PreferenceManager.getDefaultSharedPreferences(CitraApplication.appContext)

    val swapBYEnabled: Boolean
        get() = preferences.getBoolean(Settings.PREF_SWAP_BUTTONS_B_Y, false)

    /**
     * Returns the guest button to emit for a resolved binding, swapping
     * B <-> Y when the option is enabled. All other buttons pass through.
     */
    fun applyBYSwap(button: Int): Int {
        if (!swapBYEnabled) {
            return button
        }
        return when (button) {
            NativeLibrary.ButtonType.BUTTON_B -> NativeLibrary.ButtonType.BUTTON_Y
            NativeLibrary.ButtonType.BUTTON_Y -> NativeLibrary.ButtonType.BUTTON_B
            else -> button
        }
    }
}
