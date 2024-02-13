package org.citra.citra_emu.vr

import android.view.Surface
import androidx.annotation.Keep
import org.citra.citra_emu.R
import org.citra.citra_emu.fragments.EmulationFragment
import org.citra.citra_emu.utils.Log

/*
 * This class provides utils for the GameSurfaceLayer.  Its current function is
 *to pass the swapchain's surface to the underlying native code.
 *
 * Note: this is set up to require the min number of changes possible to
 *existing Citra code, in case an upstream merge is desired.
 **/
@Keep
object GameSurfaceLayer {

    @JvmStatic
    fun setSurface(activity: VrActivity, surface: Surface) {
        val navHostFragment = activity.supportFragmentManager!!.findFragmentById(R.id.fragment_container)
        val emulationFragment = navHostFragment!!.childFragmentManager!!.fragments!!.firstOrNull() as EmulationFragment;
        emulationFragment!!.surfaceCreated(surface)
    }
}
