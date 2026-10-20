package org.citra.citra_emu.vr.utils

import android.view.KeyEvent
import androidx.preference.PreferenceManager
import org.citra.citra_emu.CitraApplication
import org.citra.citra_emu.features.settings.model.Settings

/**
 * Detects the touch controller quick save / quick load chord: hold both index
 * triggers (ZL + ZR) and press A to quick save or B to quick load.
 *
 * Touch controller buttons bypass the configurable bindings and the hotkey
 * handling, so the chord is detected on the already-translated Android key
 * codes before they reach the game. A chord press is consumed so the game
 * never sees it; buttons pressed before the chord was formed are left alone.
 *
 * The VR thread sends every press event again on each frame while a button is
 * held, so a press only counts once per physical press.
 *
 * Not thread-safe: all calls must come from the VR input thread.
 */
class VrQuickStateChord(
    private val isEnabled: () -> Boolean,
    private val onQuickSave: () -> Unit,
    private val onQuickLoad: () -> Unit
) {
    private var zlHeld = false
    private var zrHeld = false

    // Chord buttons whose press was consumed, and whose release must be too.
    private val consumed = HashSet<Int>()

    // Chord buttons already sent to the game, so that pulling both triggers
    // while one is held does not turn the held button into a chord.
    private val forwarded = HashSet<Int>()

    /** Returns true if the event was consumed and must not reach the game. */
    fun onButton(keycode: Int, isPressed: Boolean): Boolean {
        return when (keycode) {
            KeyEvent.KEYCODE_BUTTON_L2 -> {
                zlHeld = isPressed
                false
            }

            KeyEvent.KEYCODE_BUTTON_R2 -> {
                zrHeld = isPressed
                false
            }

            KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_BUTTON_B -> onActionButton(keycode, isPressed)

            else -> false
        }
    }

    private fun onActionButton(keycode: Int, isPressed: Boolean): Boolean {
        if (!isPressed) {
            forwarded.remove(keycode)
            return consumed.remove(keycode)
        }
        if (keycode in consumed) {
            return true
        }
        if (keycode in forwarded || !(zlHeld && zrHeld) || !isEnabled()) {
            forwarded.add(keycode)
            return false
        }
        consumed.add(keycode)
        if (keycode == KeyEvent.KEYCODE_BUTTON_A) onQuickSave() else onQuickLoad()
        return true
    }

    companion object {
        /** The user setting; the chord is on unless it was turned off. */
        fun isEnabledInSettings(): Boolean =
            PreferenceManager.getDefaultSharedPreferences(CitraApplication.appContext)
                .getBoolean(Settings.PREF_VR_QUICK_STATE_CHORD, true)
    }
}
