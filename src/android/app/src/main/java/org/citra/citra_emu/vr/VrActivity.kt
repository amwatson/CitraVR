package org.citra.citra_emu.vr

import android.app.Activity
import android.app.ActivityOptions
import android.content.Context
import android.content.ContextWrapper
import android.content.Intent
import android.hardware.display.DisplayManager
import android.os.Build
import android.os.Bundle
import android.view.Display
import android.view.InputDevice
import android.view.KeyEvent
import org.citra.citra_emu.NativeLibrary
import org.citra.citra_emu.activities.EmulationActivity
import org.citra.citra_emu.features.settings.ui.SettingsActivity
import org.citra.citra_emu.features.settings.utils.SettingsFile
import org.citra.citra_emu.ui.main.MainActivity
import org.citra.citra_emu.utils.Log


class VrActivity : EmulationActivity() {
    private var mHandle: Long = 0
    private var clickRunnable = ClickRunnable()

    val mVrKeyboardLauncher = registerForActivityResult(
        VrKeyboardActivity.Contract()
    ) { result: VrKeyboardActivity.Result? ->
        VrKeyboardActivity.onFinishResult(
            result!!
        )
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        Log.info("VR [Java] onCreate()");
        super.onCreate(savedInstanceState)
        if (hasRun) {
            Log.info("VR [Java] VRActivity already existed")
            finish()
            // When we detect two instances running, due to bad cleanup
            // handling, we have to terminate both of them. Restart the main
            // activity with an error message telling the user what happened.
            val relaunchMainIntent = Intent(this, MainActivity::class.java)
            relaunchMainIntent.putExtra(EXTRA_ERROR_TWO_INSTANCES, true)
            startActivity(relaunchMainIntent)
            currentActivity?.finish()
            return
        }
        hasRun = true
        currentActivity = this
        mHandle = nativeOnCreate()
    }

    override fun onDestroy() {
       Log.info("VR [Java] onDestroy");
        currentActivity = null
        if (mHandle != 0L) {
            nativeOnDestroy(mHandle)
        }
        super.onDestroy()
    }

    public override fun onStart() {
       Log.info("VR [Java] onStart");
        System.gc()
        super.onStart()
    }

    public override fun onResume() {
       Log.info("VR [Java] onResume");
        super.onResume()
    }

    public override fun onPause() {
       Log.info("VR [Java] onPause");
        super.onPause()
    }

    public override fun onStop() {
       Log.info("VR [Java] onStop");
        super.onStop()
    }

    private external fun nativeOnCreate(): Long
    private external fun nativeOnDestroy(handle: Long)
    fun finishActivity() {
        if (!isFinishing) {
            finish()
        }
    }

    fun forwardVRInput(keycode: Int, isPressed: Boolean) {
        val event = KeyEvent(
            if (isPressed) KeyEvent.ACTION_DOWN else KeyEvent.ACTION_UP, keycode
        )
        event.source = InputDevice.SOURCE_GAMEPAD
        dispatchKeyEvent(event)
    }

    fun forwardVRJoystick(x: Float, y: Float, joystickType: Int) {
        // dispatch joystick input as gamepad joystick input
        NativeLibrary.onGamePadMoveEvent(
            "Quest controller",
            if (joystickType == 0) NativeLibrary.ButtonType.STICK_C else NativeLibrary.ButtonType.STICK_LEFT,
            x, -y
        )
    }

    fun openSettingsMenu() {
        SettingsActivity.launch(this, SettingsFile.FILE_NAME_CONFIG, "")
    }

    fun sendClickToWindow(
        x: Float, y: Float,
        motionType: Int
    ) {
        clickRunnable.updateState(x, y, motionType)
        runOnUiThread(clickRunnable)
    }

    fun pauseGame() {
       Log.info("VR [Java] pauseGame");
        if (NativeLibrary.isRunning()) { NativeLibrary.pauseEmulation(); }
    }

    fun resumeGame() {
       Log.info("VR [Java] resumeGame");
        // Note: isRunning() checks to make sure the emulation has started and pausing it is
        // safe -- not whether it's paused/resumed
          if (NativeLibrary.isRunning()) { NativeLibrary.unPauseEmulation(); }
    }

    class ClickRunnable : Runnable {
        private var xPosition: Float = 0.0F
        private var yPosition: Float = 0.0F
        private var motionType: Int = 0

        fun updateState(x: Float, y: Float, motionType: Int) {
            this.xPosition = x
            this.yPosition = y
            this.motionType = motionType
        }
        override fun run() {
            when (motionType) {
                0 -> NativeLibrary.onTouchEvent(0.0F, 0.0F, false)
                1 -> NativeLibrary.onTouchEvent(xPosition, yPosition, true)
                2 -> NativeLibrary.onTouchMoved(xPosition, yPosition)
                else -> Log.error("VR [Java] sendClickToWindow: unknown motionType: $motionType")
            }
        }
    }

    companion object {
        const val EXTRA_ERROR_TWO_INSTANCES = "org.citra.citra_emu.vr.ERROR_TWO_INSTANCES"
        var hasRun = false
        var currentActivity: VrActivity? = null

        init {
            if (Build.BRAND == "oculus") {
                try {
                    System.loadLibrary("openxr_forwardloader.oculus")
                } catch (e: UnsatisfiedLinkError) {
                    // This was needed before v62
                    // In v62 this library is deleted
                }
            }
        }

        fun launch(
            context: Context, gamePath: String?,
            gameTitle: String?
        ) {
            val intent = Intent(context, VrActivity::class.java)
            val mainDisplayId = getMainDisplay(context)
            if (mainDisplayId < 0) {
                throw RuntimeException("Could not find main display")
            }
            val options = ActivityOptions.makeBasic().setLaunchDisplayId(mainDisplayId)
            intent.setFlags(
                Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK or
                        Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_SINGLE_TOP
            )
               intent.putExtra("SelectedGame", gamePath);
              intent.putExtra("SelectedTitle", gameTitle);
            if (context is ContextWrapper) {
                val baseContext = context.baseContext
                baseContext.startActivity(intent, options.toBundle())
            } else {
                context.startActivity(intent, options.toBundle())
            }
            (context as Activity).finish()
        }

        private fun getMainDisplay(context: Context): Int {
            val displayManager = context.getSystemService(DISPLAY_SERVICE) as DisplayManager
            val displays = displayManager.displays
            for (i in displays.indices) {
                if (displays[i].displayId == Display.DEFAULT_DISPLAY) {
                    return displays[i].displayId
                }
            }
            return -1
        }
    }
}
