package org.citra.citra_emu.vr.ui

import android.os.Handler
import android.widget.RadioButton
import android.widget.RadioGroup
import android.view.KeyEvent
import android.view.View
import android.widget.Button
import android.widget.ProgressBar
import android.widget.TextView
import android.widget.ToggleButton
import org.citra.citra_emu.BuildConfig
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
    if (menuTypeCurrent == MenuType.STATS) {
      endLogPerfStats()
    }
    window?.findViewById<View>(menuTypeCurrent.resId)?.visibility = View.GONE
    menuTypeCurrent = menuTypeNew
    window?.findViewById<View>(menuTypeCurrent.resId)?.visibility = View.VISIBLE
    if (menuTypeCurrent == MenuType.MAIN)
      VrMessageQueue.post(VrMessageQueue.MessageType.CHANGE_LOWER_MENU, 0)
    else if (menuTypeCurrent == MenuType.POSITION)
      VrMessageQueue.post(VrMessageQueue.MessageType.CHANGE_LOWER_MENU, 1)
    else if (menuTypeCurrent == MenuType.STATS) {
      startPerfStats()
      VrMessageQueue.post(VrMessageQueue.MessageType.CHANGE_LOWER_MENU, 2)
    }
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

  private lateinit var valueGameFps: TextView
  private lateinit var valueGameFrameTime: TextView
  private lateinit var valueEmulationSpeed: TextView
  private lateinit var valueCpuUsage: TextView
  private lateinit var cpuProgressBar : ProgressBar
  private lateinit var valueGpuUsage: TextView
  private lateinit var gpuProgressBar : ProgressBar
  private lateinit var valueAppCpu: TextView
  private lateinit var valueAppGpu: TextView
  private lateinit var valueVrLatency: TextView
  private lateinit var valueVrCompCpu: TextView
  private lateinit var valueVrCompGpu: TextView
  private lateinit var valueVrCompTears: TextView
  private lateinit var valueAppVersion: TextView
  private var perfStatsUpdater: Runnable? = null
  private lateinit var perfStatsUpdateHandler : Handler

  private external fun nativeGetStatsOXR(): FloatArray

  private fun initializeStatsPanel() {
    perfStatsUpdateHandler =  Handler(activity.mainLooper)

    valueGameFps = window?.findViewById(R.id.value_game_fps) ?: return
    valueGameFrameTime = window?.findViewById(R.id.value_game_frame_time) ?: return
    valueEmulationSpeed = window?.findViewById(R.id.value_emulation_speed) ?: return
    valueCpuUsage = window?.findViewById(R.id.value_cpu_usage) ?: return
    cpuProgressBar = window?.findViewById(R.id.progress_cpu_usage) ?: return
    valueGpuUsage = window?.findViewById(R.id.value_gpu_usage) ?: return
    gpuProgressBar = window?.findViewById(R.id.progress_gpu_usage) ?: return
    valueAppCpu = window?.findViewById(R.id.value_vr_app_cpu) ?: return
    valueAppGpu = window?.findViewById(R.id.value_vr_app_gpu) ?: return
    valueVrLatency = window?.findViewById(R.id.value_vr_app_latency) ?: return
    valueVrCompCpu= window?.findViewById(R.id.value_vr_comp_cpu) ?: return
    valueVrCompGpu= window?.findViewById(R.id.value_vr_comp_gpu) ?: return
    valueVrCompTears= window?.findViewById(R.id.value_vr_comp_tears) ?: return
    valueAppVersion = window?.findViewById(R.id.value_app_version) ?: return

    valueAppVersion.text = BuildConfig.VERSION_NAME
  }

  fun startPerfStats() {
    val SYSTEM_FPS = 0
    val FPS = 1
    val FRAMETIME = 2
    val SPEED = 3
    perfStatsUpdater = Runnable {
      val perfStats = NativeLibrary.getPerfStats()
      if (perfStats[FPS] > 0) {
        Log.info(String.format(
          "Citra Game: System FPS: %d Game FPS: %d Speed: %d%% Frame Time: %.2fms",
          (perfStats[SYSTEM_FPS] + 0.5).toInt(),
          (perfStats[FPS] + 0.5).toInt(),
          (perfStats[SPEED] * 100.0 + 0.5).toInt(),
          (perfStats[FRAMETIME] * 1000.0).toFloat(),
        ))
          valueGameFps.text = String.format("%d", (perfStats[FPS] + 0.5).toInt())
          valueGameFrameTime.text = String.format("%.2fms", (perfStats[FRAMETIME] * 1000.0).toFloat())
          valueEmulationSpeed.text = String.format("%d%%", (perfStats[SPEED] * 100.0 + 0.5).toInt())
      }

      val statsOXR: FloatArray = nativeGetStatsOXR()
      if (statsOXR.size > 0) {
        val DEVICE_CPU_USAGE = 0
        val DEVICE_GPU_USAGE = 1
        val APP_CPU_FRAMETIME_MS = 2
        val APP_GPU_FRAMETIME_MS = 3
        val APP_VR_LATENCY_MS = 4
        val APP_COMP_CPU_FRAMETIME_MS = 5
        val APP_COMP_GPU_FRAMETIME_MS = 6
        val TEAR_COUNTER = 7
        valueCpuUsage.text = String.format("%.0f%%", statsOXR[DEVICE_CPU_USAGE])
        cpuProgressBar.progress = statsOXR[DEVICE_CPU_USAGE].toInt()
        valueGpuUsage.text = String.format("%.0f%%", statsOXR[DEVICE_GPU_USAGE])
        gpuProgressBar.progress = statsOXR[DEVICE_GPU_USAGE].toInt()
        val appCpuTime = statsOXR[APP_CPU_FRAMETIME_MS]
        val appGpuTime = statsOXR[APP_GPU_FRAMETIME_MS]
        valueAppCpu.text = if (appCpuTime == 0.0f) {
          String.format("%.0fms", appCpuTime)
        } else if (appCpuTime > 0.01) {
          String.format("%.2fms", appCpuTime)
        } else {
          String.format("%.5fms", appCpuTime)
        }
        valueAppGpu.text = if (appGpuTime == 0.0f) {
          String.format("%.0fms", appGpuTime)
        } else if (appGpuTime > 0.01) {
          String.format("%.2fms", appGpuTime)
        } else {
          String.format("%.5fms", appGpuTime)
        }
        valueVrLatency.text = String.format("%.2fms", statsOXR[APP_VR_LATENCY_MS])
        valueVrCompCpu.text = String.format("%.2fms", statsOXR[APP_COMP_CPU_FRAMETIME_MS])
        valueVrCompGpu.text = String.format("%.2fms", statsOXR[APP_COMP_GPU_FRAMETIME_MS])
        valueVrCompTears.text = String.format("%d", statsOXR[TEAR_COUNTER].toInt())
      }
      perfStatsUpdateHandler.postDelayed(perfStatsUpdater!!, 3000)
    }
    perfStatsUpdateHandler.post(perfStatsUpdater!!)
  }

  fun endLogPerfStats() {
    if (perfStatsUpdater != null) {
      perfStatsUpdateHandler.removeCallbacks(perfStatsUpdater!!)
    }
  }
}
