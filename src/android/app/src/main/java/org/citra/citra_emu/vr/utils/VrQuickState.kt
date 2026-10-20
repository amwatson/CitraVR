package org.citra.citra_emu.vr.utils

import android.media.AudioManager
import android.media.ToneGenerator
import android.os.Handler
import android.os.Looper
import org.citra.citra_emu.NativeLibrary
import org.citra.citra_emu.utils.Log

/**
 * Quick save / quick load for VR, shared by the ribbon buttons, the gamepad
 * hotkeys and the touch controller chord.
 *
 * Toasts and the in-game drawer are not visible in VR, so the outcome is
 * reported with a short audio cue and, if the ribbon is showing, by its
 * status label (see [listener]).
 *
 * Saving and loading are requested from the emulation thread asynchronously,
 * so [Result.SAVING] and [Result.LOADING] mean the request was accepted.
 */
object VrQuickState {
    enum class Result {
        SAVING,
        LOADING,
        NO_QUICKSAVE
    }

    fun interface Listener {
        fun onQuickStateResult(result: Result)
    }

    /** Notified on the main thread. Only one observer (the ribbon) is needed. */
    @Volatile
    var listener: Listener? = null

    private const val TONE_VOLUME = 80
    private const val TONE_DURATION_MS = 200

    private val mainHandler = Handler(Looper.getMainLooper())
    private var toneGenerator: ToneGenerator? = null

    /** Callable from any thread. Returns false if the game is not running. */
    fun quickSave(): Boolean {
        if (!NativeLibrary.isRunning()) {
            return false
        }
        NativeLibrary.saveState(NativeLibrary.QUICKSAVE_SLOT)
        report(Result.SAVING)
        return true
    }

    /** Callable from any thread. Returns false if the game is not running. */
    fun quickLoad(): Boolean {
        if (!NativeLibrary.isRunning()) {
            return false
        }
        val result = if (NativeLibrary.loadStateIfAvailable(NativeLibrary.QUICKSAVE_SLOT)) {
            Result.LOADING
        } else {
            Result.NO_QUICKSAVE
        }
        report(result)
        return true
    }

    private fun report(result: Result) {
        playCue(result)
        mainHandler.post { listener?.onQuickStateResult(result) }
    }

    @Synchronized
    private fun playCue(result: Result) {
        if (toneGenerator == null) {
            toneGenerator = try {
                ToneGenerator(AudioManager.STREAM_MUSIC, TONE_VOLUME)
            } catch (e: RuntimeException) {
                Log.warning("VR [Java] could not create ToneGenerator: ${e.message}")
                return
            }
        }
        val tone = when (result) {
            Result.SAVING -> ToneGenerator.TONE_PROP_ACK
            Result.LOADING -> ToneGenerator.TONE_PROP_PROMPT
            Result.NO_QUICKSAVE -> ToneGenerator.TONE_PROP_NACK
        }
        toneGenerator?.startTone(tone, TONE_DURATION_MS)
    }
}
