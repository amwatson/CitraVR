package org.citra.citra_emu.vr.ui

import android.widget.Button
import android.widget.TextView
import org.citra.citra_emu.R
import org.citra.citra_emu.vr.VrActivity
import org.citra.citra_emu.vr.utils.VrMessageQueue
import java.lang.ref.WeakReference
import java.util.concurrent.atomic.AtomicBoolean

class VrErrorMessageLayer(activity: VrActivity) : VrUILayer(activity, R.layout.vr_error_window) {
    private var titleView : TextView? = null
    private var messageView: TextView? = null
    private var isSurfaceCreated : AtomicBoolean = AtomicBoolean(false)

    fun showErrorMessage(title: String, message : String) : Boolean {
        if (!isSurfaceCreated.get()) {
            return false
        }
        titleView?.text = title
        messageView?.text = message

        VrMessageQueue.post(VrMessageQueue.MessageType.SHOW_ERROR_MESSAGE, 1)
        return true
    }
    override fun onSurfaceCreated() {
        super.onSurfaceCreated()
        titleView = window!!.findViewById(R.id.title)
        messageView = window!!.findViewById(R.id.main_message)
        window!!.findViewById<Button>(R.id.abort_button).setOnClickListener { _ ->  System.exit(0)}
        window!!.findViewById<Button>(R.id.continue_button).setOnClickListener { _ ->    VrMessageQueue.post(VrMessageQueue.MessageType.SHOW_ERROR_MESSAGE, 0) }
        isSurfaceCreated.set(true)

        sVrErrorMessageLayer = WeakReference(this)
    }

    companion object {
        var sVrErrorMessageLayer : WeakReference<VrErrorMessageLayer?> = WeakReference<VrErrorMessageLayer?>(null)
    }
}