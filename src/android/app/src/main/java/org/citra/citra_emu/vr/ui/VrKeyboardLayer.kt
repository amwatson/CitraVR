package org.citra.citra_emu.vr.ui

import android.view.View
import android.widget.LinearLayout
import org.citra.citra_emu.R
import org.citra.citra_emu.utils.Log
import org.citra.citra_emu.vr.VrActivity

class VrKeyboardLayer(activity: VrActivity) : VrUILayer(activity, R.layout.vr_keyboard_abc) {

    override fun onSurfaceCreated() {
        super.onSurfaceCreated()

        window!!.decorView.findViewById<View>(R.id.keySpace).setOnClickListener { view -> Log.info("amwatson spacebar") }
        window!!.decorView.findViewById<View>(R.id.keySpace).setOnTouchListener{view, event ->
            Log.info("amwatson SPACEBAR ontouch ${event.x}, ${event.y}")
            false
        }
        var keyboard : LinearLayout = window!!.decorView.findViewById(R.id.keyboard_abc)
        keyboard!!.setOnClickListener { view -> Log.info("amwatson keyboard click ${view.x}, ${view.y}") }
        keyboard!!.setOnTouchListener { view, event ->
            Log.info("amwatson KEYBOARD ontouch ${event.x}, ${event.y} Dimens: ${view.left}, ${view.right}, ${view.top}, ${view.bottom}")
            false
        }
        window?.decorView?.setOnTouchListener { view, event ->
            // Handle the touch event
        Log.info("amwatson decorview received touch ${event.x}, ${event.y} ${event.action.toString()}")
           // keyboard.performClick()
         //   keyboard.dispatchTouchEvent(event)
            false
        }
        window?.decorView?.setOnClickListener { view -> Log.info("amwatson decorview click ${view.x}, ${view.y}") }
    }
}