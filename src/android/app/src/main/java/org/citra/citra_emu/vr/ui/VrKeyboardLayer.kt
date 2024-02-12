package org.citra.citra_emu.vr.ui

import android.view.View
import android.widget.EditText
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import org.citra.citra_emu.R
import org.citra.citra_emu.vr.VrActivity

class VrKeyboardLayer(activity: VrActivity) : VrUILayer(activity, R.layout.vr_keyboard) {

    override fun onSurfaceCreated() {
        super.onSurfaceCreated()
        var editText : EditText = window?.findViewById<View>(R.id.vrKeyboardText) as EditText
        editText!!.apply {
            // Needed to show cursor onscreen.
            requestFocus()
            WindowCompat.getInsetsController(window!!, window?.decorView!!)
                .show(WindowInsetsCompat.Type.ime())
        }
    }
}