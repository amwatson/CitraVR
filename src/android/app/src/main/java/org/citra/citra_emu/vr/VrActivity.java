package org.citra.citra_emu.vr;

import android.app.Activity;
import android.app.ActivityOptions;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.hardware.display.DisplayManager;
import android.os.Build;
import android.os.Bundle;
import android.view.Display;
import android.view.InputDevice;
import android.view.KeyEvent;
import androidx.activity.result.ActivityResultLauncher;
import org.citra.citra_emu.NativeLibrary;
import org.citra.citra_emu.activities.EmulationActivity;
import org.citra.citra_emu.applets.SoftwareKeyboard;
import org.citra.citra_emu.features.settings.ui.SettingsActivity;
import org.citra.citra_emu.features.settings.utils.SettingsFile;
import org.citra.citra_emu.utils.Log;
public class VrActivity extends EmulationActivity {

  public static final String EXTRA_ERROR_TWO_INSTANCES = "org.citra.citra_emu.vr.ERROR_TWO_INSTANCES";

    private static final String PICO_PACKAGE = "org.citra.citra_emu.pico";

    private long mHandle = 0;
    public static boolean hasRun = false;
    public static VrActivity currentActivity = null;
    ClickRunnable clickRunnable = new ClickRunnable();

    static {
        if (Build.BRAND.equals("oculus")) {
            try {
                System.loadLibrary("openxr_forwardloader.oculus");
            } catch (UnsatisfiedLinkError e) {
                // This was needed before v62
                // In v62 this library is deleted
            }
        }
    }

    public final ActivityResultLauncher<SoftwareKeyboard.KeyboardConfig> mVrKeyboardLauncher =
        registerForActivityResult(new VrKeyboardActivity.Contract(),
                                  result -> VrKeyboardActivity.onFinishResult(result));

    public static void launch(Context context, final String gamePath, final String gameTitle) {
        if (isPicoHeadset() && isPicoApkInstalled(context)) {
            launchPico(context, gamePath, gameTitle);
        } else {
            launchMeta(context, gamePath, gameTitle);
        }
    }

    private static void launchMeta(Context context, final String gamePath, final String gameTitle) {
        Intent intent = new Intent(context, VrActivity.class);
        final int mainDisplayId = getMainDisplay(context);
        if (mainDisplayId < 0) {
            throw new RuntimeException("Could not find main display");
        }
        ActivityOptions options = ActivityOptions.makeBasic().setLaunchDisplayId(mainDisplayId);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK |
                        Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        intent.putExtra(EmulationActivity.EXTRA_SELECTED_GAME, gamePath);
        intent.putExtra(EmulationActivity.EXTRA_SELECTED_TITLE, gameTitle);
        if (context instanceof ContextWrapper) {
            ContextWrapper contextWrapper = (ContextWrapper)context;
            Context baseContext = contextWrapper.getBaseContext();
            baseContext.startActivity(intent, options.toBundle());
        } else {
            context.startActivity(intent, options.toBundle());
        }
        ((Activity)(context)).finish();
    }

    private static void launchPico(Context context, final String gamePath, final String gameTitle) {
        Intent intent;
        if (context.getPackageName().compareTo(PICO_PACKAGE) == 0) {
            intent = new Intent(context, VrActivity.class);
        } else {
            intent = context.getPackageManager().getLaunchIntentForPackage(PICO_PACKAGE);
            intent.putExtra(EmulationActivity.EXTRA_LAUNCH_SELECTED, true);
        }

        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        intent.putExtra(EmulationActivity.EXTRA_SELECTED_GAME, gamePath);
        intent.putExtra(EmulationActivity.EXTRA_SELECTED_TITLE, gameTitle);
        context.startActivity(intent);
        ((Activity)(context)).finish();
    }

    private static boolean isPicoApkInstalled(Context context) {
        PackageManager pm = context.getPackageManager();
        for (ApplicationInfo app : pm.getInstalledApplications(PackageManager.GET_META_DATA)) {
            if (app.packageName.equals(PICO_PACKAGE)) {
                return true;
            }
        }
        return false;
    }

    public static boolean isPicoHeadset() {
        return Build.BRAND.compareToIgnoreCase("Pico") == 0;
    }

    public static boolean useLegacyStorage() {
        return isPicoHeadset();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Log.info("VR [Java] onCreate()");
        super.onCreate(savedInstanceState);
       if (hasRun) {
            Log.info("VR [Java] VRActivity already existed");
            finish();
            // When we detect two instances running, due to bad cleanup
            // handling, we have to terminate both of them. Restart the main
            // activity with an error message telling the user what happened.
            Intent relaunchMainIntent = new Intent(this, org.citra.citra_emu.ui.main.MainActivity.class);
            relaunchMainIntent.putExtra(EXTRA_ERROR_TWO_INSTANCES, true);
            startActivity(relaunchMainIntent);
            if (currentActivity != null) {
                currentActivity.finish();
            }
            return;
        }
        hasRun = true;
        currentActivity = this;
        mHandle = nativeOnCreate();
    }

    @Override
    protected void onDestroy() {
        Log.info("VR [Java] onDestroy");
        currentActivity = null;
        if (mHandle != 0) {
            nativeOnDestroy(mHandle);
        }
        super.onDestroy();
    }

    @Override
    public void onStart() {
        Log.info("VR [Java] onStart");
        System.gc();
        super.onStart();
    }

    @Override
    public void onResume() {
        Log.info("VR [Java] onResume");
        super.onResume();
    }

    @Override
    public void onPause() {
        Log.info("VR [Java] onPause");
        super.onPause();
    }

    @Override
    public void onStop() {
        Log.info("VR [Java] onStop");
        super.onStop();
    }

    private native long nativeOnCreate();
    private native void nativeOnDestroy(final long handle);

    private static int getMainDisplay(Context context) {
        final DisplayManager displayManager =
            (DisplayManager)context.getSystemService(Context.DISPLAY_SERVICE);
        Display[] displays = displayManager.getDisplays();
        for (int i = 0; i < displays.length; i++) {
            if (displays[i].getDisplayId() == Display.DEFAULT_DISPLAY) {
                return displays[i].getDisplayId();
            }
        }
        return -1;
    }

    public void finishActivity() {
        if (!isFinishing()) {
            finish();
        }
    }

    void forwardVRInput(final int keycode, final boolean isPressed) {
        KeyEvent event =
            new KeyEvent(isPressed ? KeyEvent.ACTION_DOWN : KeyEvent.ACTION_UP, keycode);
        event.setSource(InputDevice.SOURCE_GAMEPAD);
        dispatchKeyEvent(event);
    }

    void forwardVRJoystick(final float x, final float y, final int joystickType) {
        // dispatch joystick input as gamepad joystick input
        NativeLibrary.onGamePadMoveEvent("Quest controller",
                                         joystickType == 0 ? NativeLibrary.ButtonType.STICK_C
                                                           : NativeLibrary.ButtonType.STICK_LEFT,
                                         x, -y);
    }

    void openSettingsMenu() {
        SettingsActivity.launch(this, SettingsFile.FILE_NAME_CONFIG, "");
    }

    public void sendClickToWindow(final float x, final float y, final int motionType) {
        clickRunnable.updateState((int)x, (int)y, motionType);
        runOnUiThread(clickRunnable);
    }

    public void pauseGame() {
        Log.info("VR [Java] pauseGame");
        if (NativeLibrary.IsRunning()) {
            NativeLibrary.PauseEmulation();
        }
    }

    public void resumeGame() {
        Log.info("VR [Java] resumeGame");
        // this checks to make sure the emulation has started and pausing it is
        // safe -- not whether it's paused/resumed
        if (NativeLibrary.IsRunning()) {
            NativeLibrary.UnPauseEmulation();
        }
    }

    class ClickRunnable implements Runnable {
        private int xPosition;
        private int yPosition;
        private int motionType;

        public void updateState(int x, int y, int motionType) {
            this.xPosition = x;
            this.yPosition = y;
            this.motionType = motionType;
        }

        @Override
        public void run() {
            switch (motionType) {
            case 0:
                NativeLibrary.onTouchEvent(0, 0, false);
                break;
            case 1:
                NativeLibrary.onTouchEvent(xPosition, yPosition, true);
                break;
            case 2:
                NativeLibrary.onTouchMoved(xPosition, yPosition);
                break;
            default:
                Log.error("VR [Java] sendClickToWindow: unknown motionType: " + motionType);
                break;
            }
        }
    }

}
