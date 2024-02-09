package org.citra.citra_emu.vr.ui

import android.app.Presentation
import android.content.Context
import android.graphics.Bitmap
import android.graphics.drawable.ColorDrawable
import android.hardware.display.DisplayManager
import android.hardware.display.VirtualDisplay
import android.os.SystemClock
import android.util.DisplayMetrics
import android.view.InputDevice
import android.view.LayoutInflater
import android.view.MotionEvent
import android.view.MotionEvent.PointerCoords
import android.view.MotionEvent.PointerProperties
import android.view.Surface
import android.view.View
import android.view.Window
import android.view.WindowManager
import org.citra.citra_emu.utils.Log
import org.citra.citra_emu.vr.VrActivity
import java.io.File
import java.io.FileOutputStream
import kotlin.math.roundToInt

/*
* This class populates an "SwapchainAndroidSurfaceKHR" with the
* contents of a secondary virtual display. It allows for smooth animations,
* but the perf doesn't scale well with texture size and it doesn't support mip
*levels. Therefore, it is important to set the the display size of the texture
*(using resource sizes, display density and the native density constant) to be
*something that's close enough to 1:1 texels:pixels so as to not require mips.
**/
abstract class VrUILayer(
    val activity: VrActivity,
    private val layoutId: Int,
    densityDpi: Int = DEFAULT_DENSITY.toInt()
) {
    private val requestedDensity: Float = densityDpi.toFloat()
    private var virtualDisplay: VirtualDisplay? = null
    private var presentation: Presentation? = null

    val window: Window?
        get() = presentation!!.window

    /// Called from JNI ////
    fun getBoundsForView(handle: Long): Int {
        val contentView = LayoutInflater.from(activity).inflate(layoutId, null, false)
        if (contentView == null) {
            Log.warning("contentView is null")
            return -1
        }
        val (widthPx, heightPx) = calculateDynamicDimensions(activity, 1.0f, 1.0f)

        contentView.measure(
            View.MeasureSpec.makeMeasureSpec(widthPx, View.MeasureSpec.EXACTLY),
            View.MeasureSpec.makeMeasureSpec(heightPx, View.MeasureSpec.EXACTLY)
        )

        val measuredWidthPx = contentView.measuredWidth
        val measuredHeightPx = contentView.measuredHeight

        val displayMetrics = activity.resources.displayMetrics
        val measuredWidthDp = (measuredWidthPx / displayMetrics.density) / (DEFAULT_DENSITY / requestedDensity);
        val measuredHeightDp = measuredHeightPx / displayMetrics.density / (DEFAULT_DENSITY / requestedDensity);

        // Call native method with measured dimensions
        nativeSetBounds(handle, 0, 0, measuredWidthDp.roundToInt(), measuredHeightDp.roundToInt())
        return 0
    }


    fun sendClickToUI(x: Float, y: Float, motionType: Int): Int {
        val action = when (motionType) {
            0 -> MotionEvent.ACTION_UP
            1 -> MotionEvent.ACTION_DOWN
            2 -> MotionEvent.ACTION_MOVE
            else -> MotionEvent.ACTION_HOVER_ENTER
        }
        activity.runOnUiThread { dispatchTouchEvent(x, y, action) }
        return 0
    }

    fun setSurface(
        surface: Surface, widthDp: Int,
        heightDp: Int
    ): Int {
        activity.runOnUiThread { setSurface_(surface, widthDp, heightDp) }
        return 0
    }

    protected open fun onSurfaceCreated() {}

    private fun calculateDynamicDimensions(context: Context, widthPercentage: Float, heightPercentage: Float): Pair<Int, Int> {
        val displayMetrics = context.resources.displayMetrics
        val screenWidthPx = displayMetrics.widthPixels
        val screenHeightPx = displayMetrics.heightPixels

        val desiredWidthPx = (screenWidthPx * widthPercentage).toInt()
        val desiredHeightPx = (screenHeightPx * heightPercentage).toInt()

        return Pair(desiredWidthPx, desiredHeightPx)
    }

    private fun dispatchTouchEvent(x: Float, y: Float, action: Int) {
        val eventTime = SystemClock.uptimeMillis()
        MotionEvent.obtain(
            eventTime, // Use the same timestamp for both downTime and eventTime
            eventTime,
            action,
            1, // Only one pointer is used here
            arrayOf(PointerProperties().apply {
                id = 0
                toolType = MotionEvent.TOOL_TYPE_FINGER
            }),
            arrayOf(PointerCoords().apply {
                this.x = x
                this.y = y
                pressure = 1f
                size = 1f
            }),
            0, 0, // MetaState and buttonState
            1f, 1f, // Precision X and Y
            0, 0, // Device ID and Edge Flags
            InputDevice.SOURCE_TOUCHSCREEN,
            0 // Flags
        ).apply {
            presentation?.window?.decorView?.dispatchTouchEvent(this)
        }
    }

    private fun setSurface_(
        surface: Surface, widthDp: Int,
        heightDp: Int
    ) {
        val flags = DisplayManager.VIRTUAL_DISPLAY_FLAG_PRESENTATION
        val displayManager = activity.getSystemService(Context.DISPLAY_SERVICE) as DisplayManager
        virtualDisplay = displayManager.createVirtualDisplay(
            "CitraVR", widthDp, heightDp, requestedDensity.toInt(), surface,
            flags
        )
        presentation = Presentation(activity.applicationContext, virtualDisplay!!.display).apply {
            window?.setType(WindowManager.LayoutParams.TYPE_PRIVATE_PRESENTATION)
            setContentView(layoutId)
            // Sets the background to transparent. Remove to set background to black
            // (useful for catching overrendering)
            window?.setBackgroundDrawable(ColorDrawable(0))
            show()
        }
        onSurfaceCreated()
    }

    fun writeBitmapToDisk(bmp: Bitmap, outName: String?) {
        val sdCard = activity.externalCacheDir
        if (sdCard != null && outName != null) {
            val file = File(sdCard.absolutePath, outName)
            try {
                FileOutputStream(file).use { out ->
                    bmp.compress(Bitmap.CompressFormat.PNG, 100, out)
                }
            } catch (e: Exception) {
                Log.error("Failed to write bitmap to disk: ${e.message}")
            }
        }
    }

    companion object {
        // DPI android uses as "1:1" with dp coordinates.
        // AKA "baseline density"
        private const val DEFAULT_DENSITY = DisplayMetrics.DENSITY_MEDIUM.toFloat()
        private external fun nativeSetBounds(
            handle: Long, leftInDp: Int, topInDp: Int,
            rightInDp: Int, bottomInDp: Int
        )

        /*** Debug/Testing  */
        const val DEBUG_WRITE_VIEW_TO_DISK = false
    }
}