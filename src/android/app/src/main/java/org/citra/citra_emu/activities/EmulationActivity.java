package org.citra.citra_emu.activities;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.util.Pair;
import android.util.SparseIntArray;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.SubMenu;
import android.view.View;
import android.widget.CheckBox;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.result.ActivityResultLauncher;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.PopupMenu;
import androidx.core.app.NotificationManagerCompat;
import androidx.fragment.app.FragmentActivity;

import org.citra.citra_emu.CitraApplication;
import org.citra.citra_emu.NativeLibrary;
import org.citra.citra_emu.R;
import org.citra.citra_emu.contracts.OpenFileResultContract;
import org.citra.citra_emu.features.cheats.ui.CheatsActivity;
import org.citra.citra_emu.features.settings.model.SettingSection;
import org.citra.citra_emu.features.settings.model.Settings;
import org.citra.citra_emu.features.settings.model.StringSetting;
import org.citra.citra_emu.features.settings.model.view.InputBindingSetting;
import org.citra.citra_emu.features.settings.ui.SettingsActivity;
import org.citra.citra_emu.features.settings.utils.SettingsFile;
import org.citra.citra_emu.camera.StillImageCameraHelper;
import org.citra.citra_emu.fragments.EmulationFragment;
import org.citra.citra_emu.utils.ControllerMappingHelper;
import org.citra.citra_emu.utils.EmulationMenuSettings;
import org.citra.citra_emu.utils.FileBrowserHelper;
import org.citra.citra_emu.utils.ForegroundService;
import org.citra.citra_emu.utils.Log;
import org.citra.citra_emu.utils.ThemeUtil;

import java.lang.annotation.Retention;
import java.util.Collections;
import java.util.List;

import static android.Manifest.permission.CAMERA;
import static android.Manifest.permission.RECORD_AUDIO;
import static java.lang.annotation.RetentionPolicy.SOURCE;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.slider.Slider;

public class EmulationActivity extends AppCompatActivity {
    public static final String EXTRA_SELECTED_GAME = "SelectedGame";
    public static final String EXTRA_SELECTED_TITLE = "SelectedTitle";
    public static final int MENU_ACTION_EDIT_CONTROLS_PLACEMENT = 0;
    public static final int MENU_ACTION_TOGGLE_CONTROLS = 1;
    public static final int MENU_ACTION_ADJUST_SCALE = 2;
    public static final int MENU_ACTION_EXIT = 3;
    public static final int MENU_ACTION_SHOW_FPS = 4;
    public static final int MENU_ACTION_SCREEN_LAYOUT_LANDSCAPE = 5;
    public static final int MENU_ACTION_SCREEN_LAYOUT_PORTRAIT = 6;
    public static final int MENU_ACTION_SCREEN_LAYOUT_SINGLE = 7;
    public static final int MENU_ACTION_SCREEN_LAYOUT_SIDEBYSIDE = 8;
    public static final int MENU_ACTION_SWAP_SCREENS = 9;
    public static final int MENU_ACTION_RESET_OVERLAY = 10;
    public static final int MENU_ACTION_SHOW_OVERLAY = 11;
    public static final int MENU_ACTION_OPEN_SETTINGS = 12;
    public static final int MENU_ACTION_LOAD_AMIIBO = 13;
    public static final int MENU_ACTION_REMOVE_AMIIBO = 14;
    public static final int MENU_ACTION_JOYSTICK_REL_CENTER = 15;
    public static final int MENU_ACTION_DPAD_SLIDE_ENABLE = 16;
    public static final int MENU_ACTION_OPEN_CHEATS = 17;
    public static final int MENU_ACTION_CLOSE_GAME = 18;

    public static final int REQUEST_SELECT_AMIIBO = 2;
    private static final int EMULATION_RUNNING_NOTIFICATION = 0x1000;
    private static SparseIntArray buttonsActionsMap = new SparseIntArray();

    private final ActivityResultLauncher<Boolean> mOpenFileLauncher =
        registerForActivityResult(new OpenFileResultContract(), result -> {
            if (result == null)
                return;
            String[] selectedFiles = FileBrowserHelper.getSelectedFiles(
                result, getApplicationContext(), Collections.singletonList("bin"));
            if (selectedFiles == null)
                return;

            onAmiiboSelected(selectedFiles[0]);
        });

    static {
        buttonsActionsMap.append(R.id.menu_emulation_edit_layout,
                EmulationActivity.MENU_ACTION_EDIT_CONTROLS_PLACEMENT);
        buttonsActionsMap.append(R.id.menu_emulation_toggle_controls,
                EmulationActivity.MENU_ACTION_TOGGLE_CONTROLS);
        buttonsActionsMap
                .append(R.id.menu_emulation_adjust_scale, EmulationActivity.MENU_ACTION_ADJUST_SCALE);
        buttonsActionsMap.append(R.id.menu_emulation_show_fps,
                EmulationActivity.MENU_ACTION_SHOW_FPS);
        buttonsActionsMap.append(R.id.menu_screen_layout_landscape,
                EmulationActivity.MENU_ACTION_SCREEN_LAYOUT_LANDSCAPE);
        buttonsActionsMap.append(R.id.menu_screen_layout_portrait,
                EmulationActivity.MENU_ACTION_SCREEN_LAYOUT_PORTRAIT);
        buttonsActionsMap.append(R.id.menu_screen_layout_single,
                EmulationActivity.MENU_ACTION_SCREEN_LAYOUT_SINGLE);
        buttonsActionsMap.append(R.id.menu_screen_layout_sidebyside,
                EmulationActivity.MENU_ACTION_SCREEN_LAYOUT_SIDEBYSIDE);
        buttonsActionsMap.append(R.id.menu_emulation_swap_screens,
                EmulationActivity.MENU_ACTION_SWAP_SCREENS);
        buttonsActionsMap
                .append(R.id.menu_emulation_reset_overlay, EmulationActivity.MENU_ACTION_RESET_OVERLAY);
        buttonsActionsMap
                .append(R.id.menu_emulation_show_overlay, EmulationActivity.MENU_ACTION_SHOW_OVERLAY);
        buttonsActionsMap
                .append(R.id.menu_emulation_open_settings, EmulationActivity.MENU_ACTION_OPEN_SETTINGS);
        buttonsActionsMap
                .append(R.id.menu_emulation_amiibo_load, EmulationActivity.MENU_ACTION_LOAD_AMIIBO);
        buttonsActionsMap
                .append(R.id.menu_emulation_amiibo_remove, EmulationActivity.MENU_ACTION_REMOVE_AMIIBO);
        buttonsActionsMap.append(R.id.menu_emulation_joystick_rel_center,
                EmulationActivity.MENU_ACTION_JOYSTICK_REL_CENTER);
        buttonsActionsMap.append(R.id.menu_emulation_dpad_slide_enable,
                EmulationActivity.MENU_ACTION_DPAD_SLIDE_ENABLE);
        buttonsActionsMap
                .append(R.id.menu_emulation_open_cheats, EmulationActivity.MENU_ACTION_OPEN_CHEATS);
        buttonsActionsMap
                .append(R.id.menu_emulation_close_game, EmulationActivity.MENU_ACTION_CLOSE_GAME);
    }

    private EmulationFragment mEmulationFragment;
    private SharedPreferences mPreferences;
    private ControllerMappingHelper mControllerMappingHelper;
    private Intent foregroundService;
    private boolean activityRecreated;
    private String mSelectedTitle;
    private String mPath;

    // settings state when the activity initializes
    private Settings mInitSettings = new Settings();
    private SettingSection mControlsSection = null;


    public static void launch(FragmentActivity activity, String path, String title) {
        Intent launcher = new Intent(activity, EmulationActivity.class);

        launcher.putExtra(EXTRA_SELECTED_GAME, path);
        launcher.putExtra(EXTRA_SELECTED_TITLE, title);
        activity.startActivity(launcher);
    }

    public static void tryDismissRunningNotification(Activity activity) {
        NotificationManagerCompat.from(activity).cancel(EMULATION_RUNNING_NOTIFICATION);
    }

    @Override
    protected void onDestroy() {
        stopService(foregroundService);
        super.onDestroy();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        ThemeUtil.applyTheme(this);

        super.onCreate(savedInstanceState);

        if (savedInstanceState == null) {
            // Get params we were passed
            Intent gameToEmulate = getIntent();
            mPath = gameToEmulate.getStringExtra(EXTRA_SELECTED_GAME);
            mSelectedTitle = gameToEmulate.getStringExtra(EXTRA_SELECTED_TITLE);
            activityRecreated = false;
        } else {
            activityRecreated = true;
            restoreState(savedInstanceState);
        }

        mControllerMappingHelper = new ControllerMappingHelper();

        // Set these options now so that the SurfaceView the game renders into is the right size.
        enableFullscreenImmersive();

        setContentView(R.layout.activity_emulation);

        // Find or create the EmulationFragment
        mEmulationFragment = (EmulationFragment) getSupportFragmentManager()
                .findFragmentById(R.id.frame_emulation_fragment);
        if (mEmulationFragment == null) {
            mEmulationFragment = EmulationFragment.newInstance(mPath);
            getSupportFragmentManager().beginTransaction()
                    .add(R.id.frame_emulation_fragment, mEmulationFragment)
                    .commit();
        }

        setTitle(mSelectedTitle);

        mPreferences = PreferenceManager.getDefaultSharedPreferences(this);
        mInitSettings.loadSettings(null);
        mControlsSection = mInitSettings.getSection(Settings.SECTION_CONTROLS);

        // Start a foreground service to prevent the app from getting killed in the background
        foregroundService = new Intent(EmulationActivity.this, ForegroundService.class);
        startForegroundService(foregroundService);

        // Override Citra core INI with the one set by our in game menu
        NativeLibrary.SwapScreens(EmulationMenuSettings.getSwapScreens(),
                getWindowManager().getDefaultDisplay().getRotation());
    }

    @Override
    protected void onSaveInstanceState(@NonNull Bundle outState) {
        outState.putString(EXTRA_SELECTED_GAME, mPath);
        outState.putString(EXTRA_SELECTED_TITLE, mSelectedTitle);
        super.onSaveInstanceState(outState);
    }

    protected void restoreState(Bundle savedInstanceState) {
        mPath = savedInstanceState.getString(EXTRA_SELECTED_GAME);
        mSelectedTitle = savedInstanceState.getString(EXTRA_SELECTED_TITLE);

        // If an alert prompt was in progress when state was restored, retry displaying it
        NativeLibrary.retryDisplayAlertPrompt();
    }

    @Override
    public void onRestart() {
        super.onRestart();
        NativeLibrary.ReloadCameraDevices();
    }

    @Override
    public void onBackPressed() {
        View anchor = findViewById(R.id.menu_anchor);
        PopupMenu popupMenu = new PopupMenu(this, anchor);
        onCreateOptionsMenu(popupMenu.getMenu(), popupMenu.getMenuInflater());
        updateSavestateMenuOptions(popupMenu.getMenu());
        popupMenu.setOnMenuItemClickListener(this::onOptionsItemSelected);
        popupMenu.show();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        switch (requestCode) {
            case NativeLibrary.REQUEST_CODE_NATIVE_CAMERA:
                if (grantResults[0] != PackageManager.PERMISSION_GRANTED &&
                        shouldShowRequestPermissionRationale(CAMERA)) {
                    new MaterialAlertDialogBuilder(this)
                            .setTitle(R.string.camera)
                            .setMessage(R.string.camera_permission_needed)
                            .setPositiveButton(android.R.string.ok, null)
                            .show();
                }
                NativeLibrary.CameraPermissionResult(grantResults[0] == PackageManager.PERMISSION_GRANTED);
                break;
            case NativeLibrary.REQUEST_CODE_NATIVE_MIC:
                if (grantResults[0] != PackageManager.PERMISSION_GRANTED &&
                        shouldShowRequestPermissionRationale(RECORD_AUDIO)) {
                    new MaterialAlertDialogBuilder(this)
                            .setTitle(R.string.microphone)
                            .setMessage(R.string.microphone_permission_needed)
                            .setPositiveButton(android.R.string.ok, null)
                            .show();
                }
                NativeLibrary.MicPermissionResult(grantResults[0] == PackageManager.PERMISSION_GRANTED);
                break;
            default:
                super.onRequestPermissionsResult(requestCode, permissions, grantResults);
                break;
        }
    }

    public void onEmulationStarted() {
        Toast.makeText(this, getString(R.string.emulation_menu_help), Toast.LENGTH_LONG).show();
    }

    private void enableFullscreenImmersive() {
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
                        View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                        View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                        View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                        View.SYSTEM_UI_FLAG_FULLSCREEN |
                        View.SYSTEM_UI_FLAG_IMMERSIVE |
                        View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        onCreateOptionsMenu(menu, getMenuInflater());
        return true;
    }

    private void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        inflater.inflate(R.menu.menu_emulation, menu);

        int layoutOptionMenuItem = R.id.menu_screen_layout_landscape;
        switch (EmulationMenuSettings.getLandscapeScreenLayout()) {
            case EmulationMenuSettings.LayoutOption_SingleScreen:
                layoutOptionMenuItem = R.id.menu_screen_layout_single;
                break;
            case EmulationMenuSettings.LayoutOption_SideScreen:
                layoutOptionMenuItem = R.id.menu_screen_layout_sidebyside;
                break;
            case EmulationMenuSettings.LayoutOption_MobilePortrait:
                layoutOptionMenuItem = R.id.menu_screen_layout_portrait;
                break;
        }

        menu.findItem(layoutOptionMenuItem).setChecked(true);
        menu.findItem(R.id.menu_emulation_joystick_rel_center).setChecked(EmulationMenuSettings.getJoystickRelCenter());
        menu.findItem(R.id.menu_emulation_dpad_slide_enable).setChecked(EmulationMenuSettings.getDpadSlideEnable());
        menu.findItem(R.id.menu_emulation_show_fps).setChecked(EmulationMenuSettings.getShowFps());
        menu.findItem(R.id.menu_emulation_swap_screens).setChecked(EmulationMenuSettings.getSwapScreens());
        menu.findItem(R.id.menu_emulation_show_overlay).setChecked(EmulationMenuSettings.getShowOverlay());
    }

    private void DisplaySavestateWarning() {
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(CitraApplication.getAppContext());
        if (preferences.getBoolean("savestateWarningShown", false)) {
            return;
        }

        LayoutInflater inflater = mEmulationFragment.requireActivity().getLayoutInflater();
        View view = inflater.inflate(R.layout.dialog_checkbox, null);
        CheckBox checkBox = view.findViewById(R.id.checkBox);

        new MaterialAlertDialogBuilder(this)
                .setTitle(R.string.savestate_warning_title)
                .setMessage(R.string.savestate_warning_message)
                .setView(view)
                .setPositiveButton(android.R.string.ok, (dialog, which) -> {
                    preferences.edit().putBoolean("savestateWarningShown", checkBox.isChecked()).apply();
                })
                .show();
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        super.onPrepareOptionsMenu(menu);
        updateSavestateMenuOptions(menu);
        return true;
    }

    private void updateSavestateMenuOptions(Menu menu) {
        final NativeLibrary.SavestateInfo[] savestates = NativeLibrary.GetSavestateInfo();
        if (savestates == null) {
            menu.findItem(R.id.menu_emulation_save_state).setVisible(false);
            menu.findItem(R.id.menu_emulation_load_state).setVisible(false);
            return;
        }
        menu.findItem(R.id.menu_emulation_save_state).setVisible(true);
        menu.findItem(R.id.menu_emulation_load_state).setVisible(true);

        final SubMenu saveStateMenu = menu.findItem(R.id.menu_emulation_save_state).getSubMenu();
        final SubMenu loadStateMenu = menu.findItem(R.id.menu_emulation_load_state).getSubMenu();
        saveStateMenu.clear();
        loadStateMenu.clear();

        // Update savestates information
        for (int i = 0; i < NativeLibrary.SAVESTATE_SLOT_COUNT; ++i) {
            final int slot = i + 1;
            final String text = getString(R.string.emulation_empty_state_slot, slot);
            saveStateMenu.add(text).setEnabled(true).setOnMenuItemClickListener((item) -> {
                DisplaySavestateWarning();
                NativeLibrary.SaveState(slot);
                return true;
            });
            loadStateMenu.add(text).setEnabled(false).setOnMenuItemClickListener((item) -> {
                NativeLibrary.LoadState(slot);
                return true;
            });
        }
        for (final NativeLibrary.SavestateInfo info : savestates) {
            final String text = getString(R.string.emulation_occupied_state_slot, info.slot, info.time);
            saveStateMenu.getItem(info.slot - 1).setTitle(text);
            loadStateMenu.getItem(info.slot - 1).setTitle(text).setEnabled(true);
        }
    }

    @SuppressWarnings("WrongConstant")
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int action = buttonsActionsMap.get(item.getItemId(), -1);

        switch (action) {
            // Edit the placement of the controls
            case MENU_ACTION_EDIT_CONTROLS_PLACEMENT:
                editControlsPlacement();
                break;

            // Enable/Disable specific buttons or the entire input overlay.
            case MENU_ACTION_TOGGLE_CONTROLS:
                toggleControls();
                break;

            // Adjust the scale of the overlay controls.
            case MENU_ACTION_ADJUST_SCALE:
                adjustScale();
                break;

            // Toggle the visibility of the Performance stats TextView
            case MENU_ACTION_SHOW_FPS: {
                final boolean isEnabled = !EmulationMenuSettings.getShowFps();
                EmulationMenuSettings.setShowFps(isEnabled);
                item.setChecked(isEnabled);

                mEmulationFragment.updateShowFpsOverlay();
                break;
            }
            // Sets the screen layout to Landscape
            case MENU_ACTION_SCREEN_LAYOUT_LANDSCAPE:
                changeScreenOrientation(EmulationMenuSettings.LayoutOption_MobileLandscape, item);
                break;

            // Sets the screen layout to Portrait
            case MENU_ACTION_SCREEN_LAYOUT_PORTRAIT:
                changeScreenOrientation(EmulationMenuSettings.LayoutOption_MobilePortrait, item);
                break;

            // Sets the screen layout to Single
            case MENU_ACTION_SCREEN_LAYOUT_SINGLE:
                changeScreenOrientation(EmulationMenuSettings.LayoutOption_SingleScreen, item);
                break;

            // Sets the screen layout to Side by Side
            case MENU_ACTION_SCREEN_LAYOUT_SIDEBYSIDE:
                changeScreenOrientation(EmulationMenuSettings.LayoutOption_SideScreen, item);
                break;

            // Swap the top and bottom screen locations
            case MENU_ACTION_SWAP_SCREENS: {
                final boolean isEnabled = !EmulationMenuSettings.getSwapScreens();
                EmulationMenuSettings.setSwapScreens(isEnabled);
                item.setChecked(isEnabled);

                NativeLibrary.SwapScreens(isEnabled, getWindowManager().getDefaultDisplay()
                        .getRotation());
                break;
            }

            // Reset overlay placement
            case MENU_ACTION_RESET_OVERLAY:
                resetOverlay();
                break;

            // Show or hide overlay
            case MENU_ACTION_SHOW_OVERLAY: {
                final boolean isEnabled = !EmulationMenuSettings.getShowOverlay();
                EmulationMenuSettings.setShowOverlay(isEnabled);
                item.setChecked(isEnabled);

                mEmulationFragment.refreshInputOverlay();
                break;
            }

            case MENU_ACTION_EXIT:
                mEmulationFragment.stopEmulation();
                finish();
                break;

            case MENU_ACTION_OPEN_SETTINGS:
                SettingsActivity.launch(this, SettingsFile.FILE_NAME_CONFIG, "");
                break;

            case MENU_ACTION_LOAD_AMIIBO:
                mOpenFileLauncher.launch(false);
                break;

            case MENU_ACTION_REMOVE_AMIIBO:
                RemoveAmiibo();
                break;

            case MENU_ACTION_JOYSTICK_REL_CENTER:
                final boolean isJoystickRelCenterEnabled = !EmulationMenuSettings.getJoystickRelCenter();
                EmulationMenuSettings.setJoystickRelCenter(isJoystickRelCenterEnabled);
                item.setChecked(isJoystickRelCenterEnabled);
                break;

            case MENU_ACTION_DPAD_SLIDE_ENABLE:
                final boolean isDpadSlideEnabled = !EmulationMenuSettings.getDpadSlideEnable();
                EmulationMenuSettings.setDpadSlideEnable(isDpadSlideEnabled);
                item.setChecked(isDpadSlideEnabled);
                break;

            case MENU_ACTION_OPEN_CHEATS:
                CheatsActivity.launch(this, NativeLibrary.GetRunningTitleId());
                break;

            case MENU_ACTION_CLOSE_GAME:
                NativeLibrary.PauseEmulation();
                new MaterialAlertDialogBuilder(this)
                        .setTitle(R.string.emulation_close_game)
                        .setMessage(R.string.emulation_close_game_message)
                        .setPositiveButton(android.R.string.yes, (dialogInterface, i) ->
                        {
                            mEmulationFragment.stopEmulation();
                            finish();
                        })
                        .setNegativeButton(android.R.string.cancel, (dialogInterface, i) -> NativeLibrary.UnPauseEmulation())
                        .setOnCancelListener(dialogInterface -> NativeLibrary.UnPauseEmulation())
                        .show();
                break;
        }

        return true;
    }

    private void changeScreenOrientation(int layoutOption, MenuItem item) {
        item.setChecked(true);
        NativeLibrary.NotifyOrientationChange(layoutOption, getWindowManager().getDefaultDisplay()
                .getRotation());
        EmulationMenuSettings.setLandscapeScreenLayout(layoutOption);
    }

    private void editControlsPlacement() {
        if (mEmulationFragment.isConfiguringControls()) {
            mEmulationFragment.stopConfiguringControls();
        } else {
            mEmulationFragment.startConfiguringControls();
        }
    }

    private int getConfigButton(final int nativeButton) {
        if (mControlsSection == null) {
            return -1;
        }
        final String configKey = InputBindingSetting.getButtonKey(nativeButton);
        if (configKey == null || configKey.isEmpty()) {
            return -2;
        }
        final StringSetting configButtonStrSetting = (StringSetting)mControlsSection.getSetting(configKey);
        if (configButtonStrSetting == null || configButtonStrSetting.getValue().isEmpty()) {
            return -3;
        }
        final String configButtonStr = configButtonStrSetting.getValue();
        String[] keyValuePairs = configButtonStr.split(",");
        String[] codePair = keyValuePairs[1].split(":");
        int code;
        try {
            code = Integer.parseInt(codePair[1]);
        } catch (Exception e) {
            return -4;
        }
        return code;
    }

    // Gets button presses
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        int action;
        int button = mPreferences.getInt(InputBindingSetting.getInputButtonKey(event.getKeyCode()), event.getKeyCode());
        final boolean hasPreference = button != event.getKeyCode();
        if (hasPreference ) {
            final int configButton = getConfigButton(button);
           if (configButton > 0) {
               button = configButton;
           }
        }

        switch (event.getAction()) {
            case KeyEvent.ACTION_DOWN:
                // Handling the case where the back button is pressed.
                if (event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
                    onBackPressed();
                    return true;
                }

                // Normal key events.
                action = NativeLibrary.ButtonState.PRESSED;
                break;
            case KeyEvent.ACTION_UP:
                action = NativeLibrary.ButtonState.RELEASED;
                break;
            default:
                return false;
        }

        if (event.getKeyCode() == KeyEvent.KEYCODE_DPAD_UP) {
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_UP, action);
        } else if (event.getKeyCode() == KeyEvent.KEYCODE_DPAD_DOWN) {
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_DOWN, action);
        } else if (event.getKeyCode() == KeyEvent.KEYCODE_DPAD_LEFT) {
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_LEFT, action);
        } else if (event.getKeyCode() == KeyEvent.KEYCODE_DPAD_RIGHT) {
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_RIGHT, action);
        }

        InputDevice input = event.getDevice();

        if (input == null) {
            // Controller was disconnected
            return false;
        }

        return NativeLibrary.onGamePadEvent(input.getDescriptor(), button, action);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent result) {
        super.onActivityResult(requestCode, resultCode, result);
        if (requestCode == StillImageCameraHelper.REQUEST_CAMERA_FILE_PICKER) {
            StillImageCameraHelper.OnFilePickerResult(resultCode == RESULT_OK ? result : null);
        }
    }

    private void onAmiiboSelected(String selectedFile) {
        boolean success = NativeLibrary.LoadAmiibo(selectedFile);

        if (!success) {
            new MaterialAlertDialogBuilder(this)
                    .setTitle(R.string.amiibo_load_error)
                    .setMessage(R.string.amiibo_load_error_message)
                    .setPositiveButton(android.R.string.ok, null)
                    .show();
        }
    }

    private void RemoveAmiibo() {
        NativeLibrary.RemoveAmiibo();
    }

    private void toggleControls() {
        final SharedPreferences.Editor editor = mPreferences.edit();
        boolean[] enabledButtons = new boolean[14];

        for (int i = 0; i < enabledButtons.length; i++) {
            // Buttons that are disabled by default
            boolean defaultValue = true;
            switch (i) {
                case 6: // ZL
                case 7: // ZR
                case 12: // C-stick
                    defaultValue = false;
                    break;
            }

            enabledButtons[i] = mPreferences.getBoolean("buttonToggle" + i, defaultValue);
        }

        new MaterialAlertDialogBuilder(this)
                .setTitle(R.string.emulation_toggle_controls)
                .setMultiChoiceItems(R.array.n3dsButtons, enabledButtons,
                        (dialog, indexSelected, isChecked) -> editor
                                .putBoolean("buttonToggle" + indexSelected, isChecked))
                .setPositiveButton(android.R.string.ok, (dialogInterface, i) ->
                {
                    editor.apply();
                    mEmulationFragment.refreshInputOverlay();
                })
                .show();
    }

    private void adjustScale() {
        LayoutInflater inflater = LayoutInflater.from(this);
        View view = inflater.inflate(R.layout.dialog_slider, null);

        final Slider slider = view.findViewById(R.id.slider);
        final TextView textValue = view.findViewById(R.id.text_value);
        final TextView units = view.findViewById(R.id.text_units);

        slider.setValueTo(150);
        slider.setValue(mPreferences.getInt("controlScale", 50));
        slider.addOnChangeListener((slider1, progress, fromUser) -> {
            textValue.setText(String.valueOf((int) progress + 50));
            setControlScale((int) slider1.getValue());
        });

        textValue.setText(String.valueOf((int) slider.getValue() + 50));
        units.setText("%");

        final int previousProgress = (int) slider.getValue();

        new MaterialAlertDialogBuilder(this)
                .setTitle(R.string.emulation_control_scale)
                .setView(view)
                .setNegativeButton(android.R.string.cancel, (dialogInterface, i) -> setControlScale(previousProgress))
                .setPositiveButton(android.R.string.ok, (dialogInterface, i) -> setControlScale((int) slider.getValue()))
                .setNeutralButton(R.string.slider_default, (dialogInterface, i) -> setControlScale(50))
                .show();
    }

    private void setControlScale(int scale) {
        SharedPreferences.Editor editor = mPreferences.edit();
        editor.putInt("controlScale", scale);
        editor.apply();
        mEmulationFragment.refreshInputOverlay();
    }

    private void resetOverlay() {
        new MaterialAlertDialogBuilder(this)
                .setTitle(getString(R.string.emulation_touch_overlay_reset))
                .setPositiveButton(android.R.string.yes, (dialogInterface, i) -> mEmulationFragment.resetInputOverlay())
                .setNegativeButton(android.R.string.cancel, null)
                .show();
    }

    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent event) {
        if (((event.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) == 0)) {
            return super.dispatchGenericMotionEvent(event);
        }

        // Don't attempt to do anything if we are disconnecting a device.
        if (event.getActionMasked() == MotionEvent.ACTION_CANCEL) {
            return true;
        }

        InputDevice input = event.getDevice();
        List<InputDevice.MotionRange> motions = input.getMotionRanges();

        float[] axisValuesCirclePad = {0.0f, 0.0f};
        float[] axisValuesCStick = {0.0f, 0.0f};
        float[] axisValuesDPad = {0.0f, 0.0f};
        boolean isTriggerPressedLMapped = false;
        boolean isTriggerPressedRMapped = false;
        boolean isTriggerPressedZLMapped = false;
        boolean isTriggerPressedZRMapped = false;
        boolean isTriggerPressedL = false;
        boolean isTriggerPressedR = false;
        boolean isTriggerPressedZL = false;
        boolean isTriggerPressedZR = false;

        for (InputDevice.MotionRange range : motions) {
            int axis = range.getAxis();
            float origValue = event.getAxisValue(axis);
            float value = mControllerMappingHelper.scaleAxis(input, axis, origValue);
            int nextMapping = mPreferences.getInt(InputBindingSetting.getInputAxisButtonKey(axis), -1);
            int guestOrientation = mPreferences.getInt(InputBindingSetting.getInputAxisOrientationKey(axis), -1);

            // add some defaults since other sources don't really take them
            if (nextMapping == -1 || guestOrientation == -1) {
                if (axis == 14) /* right joystick up/down */ {
                    nextMapping = NativeLibrary.ButtonType.STICK_C;
                    guestOrientation = 1;
                } else if (axis == 11) /* right joystick left/right */ {
                    nextMapping = NativeLibrary.ButtonType.STICK_C;
                    guestOrientation = 0;
                } else if (axis == 1) /* left joystick up/down */ {
                    nextMapping = NativeLibrary.ButtonType.STICK_LEFT;
                    guestOrientation = 1;
                } else if (axis == 0) /* left joystick left/right */ {
                    nextMapping = NativeLibrary.ButtonType.STICK_LEFT;
                    guestOrientation = 0;
                } else {
                    // Axis is unmapped
                    continue;
                }
            }

            if ((value > 0.f && value < 0.1f) || (value < 0.f && value > -0.1f)) {
                // Skip joystick wobble
                value = 0.f;
            }

            if (nextMapping == NativeLibrary.ButtonType.STICK_LEFT) {
                axisValuesCirclePad[guestOrientation] = value;
            } else if (nextMapping == NativeLibrary.ButtonType.STICK_C) {
                axisValuesCStick[guestOrientation] = value;
            } else if (nextMapping == NativeLibrary.ButtonType.DPAD) {
                axisValuesDPad[guestOrientation] = value;
            } else if (nextMapping == NativeLibrary.ButtonType.TRIGGER_L) {
                isTriggerPressedLMapped = true;
                isTriggerPressedL = value != 0.f;
            } else if (nextMapping == NativeLibrary.ButtonType.TRIGGER_R) {
                isTriggerPressedRMapped = true;
                isTriggerPressedR = value != 0.f;
            } else if (nextMapping == NativeLibrary.ButtonType.BUTTON_ZL) {
                isTriggerPressedZLMapped = true;
                isTriggerPressedZL = value != 0.f;
            } else if (nextMapping == NativeLibrary.ButtonType.BUTTON_ZR) {
                isTriggerPressedZRMapped = true;
                isTriggerPressedZR = value != 0.f;
            }
        }

        // Circle-Pad and C-Stick status
        NativeLibrary.onGamePadMoveEvent(input.getDescriptor(), NativeLibrary.ButtonType.STICK_LEFT, axisValuesCirclePad[0], axisValuesCirclePad[1]);
        NativeLibrary.onGamePadMoveEvent(input.getDescriptor(), NativeLibrary.ButtonType.STICK_C, axisValuesCStick[0], axisValuesCStick[1]);

        // Triggers L/R and ZL/ZR
        if (isTriggerPressedLMapped) {
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.TRIGGER_L, isTriggerPressedL ? NativeLibrary.ButtonState.PRESSED : NativeLibrary.ButtonState.RELEASED);
        }
        if (isTriggerPressedRMapped) {
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.TRIGGER_R, isTriggerPressedR ? NativeLibrary.ButtonState.PRESSED : NativeLibrary.ButtonState.RELEASED);
        }
        if (isTriggerPressedZLMapped) {
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.BUTTON_ZL, isTriggerPressedZL ? NativeLibrary.ButtonState.PRESSED : NativeLibrary.ButtonState.RELEASED);
        }
        if (isTriggerPressedZRMapped) {
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.BUTTON_ZR, isTriggerPressedZR ? NativeLibrary.ButtonState.PRESSED : NativeLibrary.ButtonState.RELEASED);
        }

        // Work-around to allow D-pad axis to be bound to emulated buttons
        if (axisValuesDPad[0] == 0.f) {
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_LEFT, NativeLibrary.ButtonState.RELEASED);
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_RIGHT, NativeLibrary.ButtonState.RELEASED);
        }
        if (axisValuesDPad[0] < 0.f) {
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_LEFT, NativeLibrary.ButtonState.PRESSED);
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_RIGHT, NativeLibrary.ButtonState.RELEASED);
        }
        if (axisValuesDPad[0] > 0.f) {
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_LEFT, NativeLibrary.ButtonState.RELEASED);
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_RIGHT, NativeLibrary.ButtonState.PRESSED);
        }
        if (axisValuesDPad[1] == 0.f) {
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_UP, NativeLibrary.ButtonState.RELEASED);
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_DOWN, NativeLibrary.ButtonState.RELEASED);
        }
        if (axisValuesDPad[1] < 0.f) {
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_UP, NativeLibrary.ButtonState.PRESSED);
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_DOWN, NativeLibrary.ButtonState.RELEASED);
        }
        if (axisValuesDPad[1] > 0.f) {
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_UP, NativeLibrary.ButtonState.RELEASED);
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_DOWN, NativeLibrary.ButtonState.PRESSED);
        }

        return true;
    }

    public boolean isActivityRecreated() {
        return activityRecreated;
    }

    @Retention(SOURCE)
    @IntDef({MENU_ACTION_EDIT_CONTROLS_PLACEMENT, MENU_ACTION_TOGGLE_CONTROLS, MENU_ACTION_ADJUST_SCALE,
            MENU_ACTION_EXIT, MENU_ACTION_SHOW_FPS, MENU_ACTION_SCREEN_LAYOUT_LANDSCAPE,
            MENU_ACTION_SCREEN_LAYOUT_PORTRAIT, MENU_ACTION_SCREEN_LAYOUT_SINGLE, MENU_ACTION_SCREEN_LAYOUT_SIDEBYSIDE,
            MENU_ACTION_SWAP_SCREENS, MENU_ACTION_RESET_OVERLAY, MENU_ACTION_SHOW_OVERLAY, MENU_ACTION_OPEN_SETTINGS})
    public @interface MenuAction {
    }
}
