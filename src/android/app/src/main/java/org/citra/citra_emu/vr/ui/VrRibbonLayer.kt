package org.citra.citra_emu.vr.ui

import android.view.KeyEvent
import android.view.View
import android.widget.Button
import android.widget.ToggleButton
import org.citra.citra_emu.NativeLibrary
import org.citra.citra_emu.R
import org.citra.citra_emu.vr.VrActivity
import org.citra.citra_emu.vr.utils.VrMessageQueue

class VrRibbonLayer(activity: VrActivity) : VrUILayer(activity, R.layout.vr_ribbon) {

    enum class MenuType(val resId: Int) {
        MAIN(R.id.main_panel),
        POSITION(R.id.position_panel)
    }

    private var menuTypeCurrent: MenuType = MenuType.MAIN

    override fun onSurfaceCreated() {
        super.onSurfaceCreated()
        initializeMainPanel()
        initializePositionalPanel()
    }

    fun switchMenus(menuTypeNew: MenuType) {
        if (menuTypeNew == menuTypeCurrent)
            return
        window?.findViewById<View>(menuTypeCurrent.resId)?.visibility = View.GONE
        menuTypeCurrent = menuTypeNew
        window?.findViewById<View>(menuTypeCurrent.resId)?.visibility = View.VISIBLE
        if (menuTypeCurrent == MenuType.MAIN)
            VrMessageQueue.post(VrMessageQueue.MessageType.CHANGE_LOWER_MENU, 0)
        else if (menuTypeCurrent == MenuType.POSITION)
            VrMessageQueue.post(VrMessageQueue.MessageType.CHANGE_LOWER_MENU, 1)
    }

    private fun initializePositionalPanel() {
        val horizontalLockToggle = window?.findViewById<ToggleButton>(R.id.horizontalAxisToggle)
        horizontalLockToggle?.setOnCheckedChangeListener { _, isChecked ->
            VrMessageQueue.post(VrMessageQueue.MessageType.CHANGE_LOCK_HORIZONTAL_AXIS, if (isChecked) 1 else 0)
        }
        val btnReset = window?.findViewById<Button>(R.id.btnReset)
        btnReset?.setOnClickListener { _ ->
            VrMessageQueue.post(VrMessageQueue.MessageType.RESET_PANEL_POSITIONS, 0)
            false
        }

        horizontalLockToggle?.isChecked = true;
        VrMessageQueue.post(VrMessageQueue.MessageType.CHANGE_LOCK_HORIZONTAL_AXIS, if (horizontalLockToggle?.isChecked == true) 1 else 0)
    }

    private fun initializeMainPanel() {
        window?.findViewById<Button>(R.id.buttonSelect)?.setOnTouchListener { _, motionEvent ->
            val action: Int = when (motionEvent.action) {
                KeyEvent.ACTION_DOWN -> {
                    // Normal key events.
                    NativeLibrary.ButtonState.PRESSED
                }

                KeyEvent.ACTION_UP -> NativeLibrary.ButtonState.RELEASED
                else -> return@setOnTouchListener false
            }
            NativeLibrary.onGamePadEvent(
                NativeLibrary.TouchScreenDevice,
                NativeLibrary.ButtonType.BUTTON_SELECT,
                action
            )
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
            NativeLibrary.onGamePadEvent(
                NativeLibrary.TouchScreenDevice,
                NativeLibrary.ButtonType.BUTTON_START,
                action
            )
            false
        }
        window?.findViewById<Button>(R.id.buttonExit)?.setOnTouchListener { _, motionEvent ->
            activity.quitToMenu()
            false
        }

        val btnNext = window?.findViewById<Button>(R.id.buttonNextMenu)
        val btnPrev = window?.findViewById<Button>(R.id.buttonPrevMenu)
        btnNext?.setOnClickListener { _ ->
            val nextIdx = (menuTypeCurrent.ordinal + 1) % MenuType.values().size
            switchMenus(MenuType.values()[nextIdx])
            if ((nextIdx + 1) >= MenuType.values().size)
                btnNext.visibility = View.INVISIBLE
            btnPrev?.visibility = View.VISIBLE
            false
        }

        btnPrev?.setOnClickListener { _ ->
            val prevIdx =
                (menuTypeCurrent.ordinal - 1 + MenuType.values().size) % MenuType.values().size
            switchMenus(MenuType.values()[prevIdx])
            if ((prevIdx - 1) <= 0)
                btnPrev.visibility = View.INVISIBLE
            btnNext?.visibility = View.VISIBLE
            false
        }
    }
}
