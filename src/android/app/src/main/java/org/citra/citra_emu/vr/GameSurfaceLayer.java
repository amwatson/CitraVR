package org.citra.citra_emu.vr;

import android.app.Activity;
import android.view.Surface;
import org.citra.citra_emu.NativeLibrary;
import org.citra.citra_emu.R;
import org.citra.citra_emu.activities.EmulationActivity;
import org.citra.citra_emu.fragments.EmulationFragment;

/*
 * This class provides utils for the GameSurfaceLayer.  Its current function is
 *to pass the swapchain's surface to the underlying native code.
 *
 * Note: this is set up to require the min number of changes possible to
 *existing Citra code, in case an upstream merge is desired.
 **/
public class GameSurfaceLayer
{
    public static void setSurface(VrActivity activity, Surface surface)
    {
        assert activity != null;
        ((EmulationFragment)activity.getSupportFragmentManager()
             .findFragmentById(R.id.frame_emulation_fragment))
            .surfaceCreated(surface);
    }
}
