package org.citra.citra_emu.vr.ui

import android.view.KeyEvent
import android.view.MotionEvent
import android.widget.Button
import android.widget.ImageButton
import org.citra.citra_emu.NativeLibrary
import org.citra.citra_emu.R
import org.citra.citra_emu.vr.VrActivity

class VrRibbonLayer(activity: VrActivity) : VrUILayer(activity, R.layout.vr_ribbon) {
       override fun onSurfaceCreated() {
        super.onSurfaceCreated()
        initializeMainView()
    }

    fun initializeMainView() {
        window?.findViewById<Button>(R.id.buttonSelect)?.setOnTouchListener { _, motionEvent ->
            val action: Int = when (motionEvent.action) {
                KeyEvent.ACTION_DOWN -> {
                    // Normal key events.
                    NativeLibrary.ButtonState.PRESSED
                }
                KeyEvent.ACTION_UP -> NativeLibrary.ButtonState.RELEASED
                else -> return@setOnTouchListener false
            }
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice,
                NativeLibrary.ButtonType.BUTTON_SELECT,
                action)
            false
        }
        window?.findViewById<Button>(R.id.buttonStart)?.setOnTouchListener { _, motionEvent ->
            val action: Int = when (motionEvent.action) {
                KeyEvent.ACTION_DOWN -> {
                    // Normal key events.
                    NativeLibrary.ButtonState.PRESSED
                }
                KeyEvent.ACTION_UP -> NativeLibrary.ButtonState.RELEASED
                else -> return@setOnTouchListener false
            }
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice,
                NativeLibrary.ButtonType.BUTTON_START,
                action)
            false
        }
        window?.findViewById<Button>(R.id.buttonExit)?.setOnTouchListener { _, motionEvent ->
            activity.quitToMenu()
            false
        }

        window?.findViewById<Button>(R.id.buttonNextMenu)?.setOnTouchListener { _, motionEvent ->

            false
        }
    }
}
