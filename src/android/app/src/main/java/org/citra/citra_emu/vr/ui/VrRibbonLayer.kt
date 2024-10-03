package org.citra.citra_emu.vr.ui

import android.widget.RadioButton
import android.widget.RadioGroup
import android.view.KeyEvent
import android.view.View
import android.widget.Button
import android.widget.ToggleButton
import org.citra.citra_emu.NativeLibrary
import org.citra.citra_emu.R
import org.citra.citra_emu.vr.VrActivity
import org.citra.citra_emu.vr.utils.VrMessageQueue
import org.citra.citra_emu.utils.Log

class VrRibbonLayer(activity: VrActivity) : VrUILayer(activity, R.layout.vr_ribbon) {

  enum class MenuType(val resId: Int, buttonId: Int) {
    MAIN(R.id.main_panel, R.id.button_menu_main),
    POSITION(R.id.position_panel, R.id.button_menu_positional),
    STATS(R.id.stats_panel, R.id.button_menu_stats)
  }

  private var menuTypeCurrent: MenuType = MenuType.MAIN

    override fun onSurfaceCreated() {
      super.onSurfaceCreated()
        initializeLeftMenu()
        initializeMainPanel()
        initializePositionalPanel()
      initializeStatsPanel()
    }

    // Used in positional menu to know when background is selected, but not the buttons,
    // in which case, move the panel.
    private var isMenuBackgroundSelected = false

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
        else if (menuTypeCurrent == MenuType.STATS)
          VrMessageQueue.post(VrMessageQueue.MessageType.CHANGE_LOWER_MENU, 2)
  }

  fun isMenuBackgroundSelected(): Boolean {
    return isMenuBackgroundSelected
  }

  private fun initializeLeftMenu() {
    val radioGroup = window?.findViewById<RadioGroup>(R.id.vertical_tab)
      radioGroup?.setOnCheckedChangeListener { group, checkedId ->
        // Loop through all radio buttons in the group
        for (i in 0 until group.childCount) {
          val btn = group.getChildAt(i) as RadioButton
            if (btn.id == checkedId) {
              // This button is checked, change the background accordingly
              btn.background = activity?.getDrawable(
                  R.drawable.vr_ribbon_button_pressed)
                when (btn.id) {
                  R.id.button_menu_main -> switchMenus(MenuType.MAIN)
                    R.id.button_menu_positional -> switchMenus(MenuType.POSITION)
                  R.id.button_menu_stats -> switchMenus(MenuType.STATS)
                }
            } else {
              // This button is not checked, revert to the default background
              btn.background = activity?.getDrawable(R.drawable.vr_ribbon_button_default)
            }
        }
      }
    // Set the first button as checked
    radioGroup?.check(R.id.button_menu_main)
  }

  private fun initializeStatsPanel() {

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

      window?.findViewById<View>(R.id.frame)?.setOnTouchListener { _, motionEvent ->
        /// set isMenuBackgroundSelected based on the motionEvent
        when (motionEvent.action) {
          KeyEvent.ACTION_DOWN -> {
            isMenuBackgroundSelected = true
          }
          KeyEvent.ACTION_UP -> {
            isMenuBackgroundSelected = false
          }
        }
          false
      }
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

  }
}
