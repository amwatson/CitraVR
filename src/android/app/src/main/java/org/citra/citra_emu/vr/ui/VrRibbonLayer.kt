package org.citra.citra_emu.vr.ui

import android.view.MotionEvent
import android.widget.Button
import android.widget.ImageButton
import org.citra.citra_emu.NativeLibrary
import org.citra.citra_emu.R
import org.citra.citra_emu.vr.VrActivity

class VrRibbonLayer(activity: VrActivity) : VrUILayer(activity, R.layout.vr_ribbon) {
       override fun onSurfaceCreated() {
        super.onSurfaceCreated()
        window?.findViewById<Button>(R.id.buttonSelect)?.setOnTouchListener { _, motionEvent ->
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice,
                NativeLibrary.ButtonType.BUTTON_SELECT,
                if (motionEvent.action == MotionEvent.ACTION_DOWN)
                    NativeLibrary.ButtonState.PRESSED
                else NativeLibrary.ButtonState.RELEASED)
            false
        }
        window?.findViewById<ImageButton>(R.id.buttonHome)?.setOnTouchListener { view, motionEvent ->
            if (motionEvent.action == MotionEvent.ACTION_DOWN) {
                (view as ImageButton).setImageResource(R.drawable.button_home_pressed)
            } else {
                (view as ImageButton).setImageResource(R.drawable.button_home)
            }
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice,
                NativeLibrary.ButtonType.BUTTON_HOME,
                if (motionEvent.action == MotionEvent.ACTION_DOWN)
                    NativeLibrary.ButtonState.PRESSED
                else NativeLibrary.ButtonState.RELEASED)
            false
        }
        window?.findViewById<Button>(R.id.buttonStart)?.setOnTouchListener { _, motionEvent ->
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice,
                NativeLibrary.ButtonType.BUTTON_START,
                if (motionEvent.action == MotionEvent.ACTION_DOWN)
                    NativeLibrary.ButtonState.PRESSED
                else NativeLibrary.ButtonState.RELEASED)
            false
        }
    }
}
