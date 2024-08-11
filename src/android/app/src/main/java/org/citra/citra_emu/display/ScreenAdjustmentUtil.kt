// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.display

import android.view.WindowManager
import org.citra.citra_emu.NativeLibrary
import org.citra.citra_emu.features.settings.model.BooleanSetting
import org.citra.citra_emu.features.settings.model.IntSetting
import org.citra.citra_emu.features.settings.model.Settings
import org.citra.citra_emu.features.settings.utils.SettingsFile
import org.citra.citra_emu.utils.EmulationMenuSettings

class ScreenAdjustmentUtil(
    private val windowManager: WindowManager,
    private val settings: Settings
) {
    fun swapScreen() {
        val isEnabled = !EmulationMenuSettings.swapScreens
        EmulationMenuSettings.swapScreens = isEnabled
        NativeLibrary.swapScreens(
            isEnabled,
            windowManager.defaultDisplay.rotation
        )
        BooleanSetting.SWAP_SCREEN.boolean = isEnabled
        settings.saveSetting(BooleanSetting.SWAP_SCREEN, SettingsFile.FILE_NAME_CONFIG)
    }
    fun cycleLayouts() {
        // TODO: figure out how to pull these from R.array
        val landscape_values = intArrayOf(6,1,3,4,0,5);
        val portrait_values = intArrayOf(0,1);
        if (NativeLibrary.isPortraitMode) {
            val current_layout = IntSetting.PORTRAIT_SCREEN_LAYOUT.int
            val pos = portrait_values.indexOf(current_layout)
            val layout_option = portrait_values[(pos + 1) % portrait_values.size]
            changePortraitOrientation(layout_option)
        } else {
            val current_layout = IntSetting.SCREEN_LAYOUT.int
            val pos = landscape_values.indexOf(current_layout)
            val layout_option = landscape_values[(pos + 1) % landscape_values.size]
            changeScreenOrientation(layout_option)
        }

    }

    fun changePortraitOrientation(layoutOption: Int) {
        IntSetting.PORTRAIT_SCREEN_LAYOUT.int = layoutOption
        settings.saveSetting(IntSetting.PORTRAIT_SCREEN_LAYOUT, SettingsFile.FILE_NAME_CONFIG)
        NativeLibrary.reloadSettings()
        NativeLibrary.updateFramebuffer(NativeLibrary.isPortraitMode)
    }

    fun changeScreenOrientation(layoutOption: Int) {
        IntSetting.SCREEN_LAYOUT.int = layoutOption
        settings.saveSetting(IntSetting.SCREEN_LAYOUT, SettingsFile.FILE_NAME_CONFIG)
        NativeLibrary.reloadSettings()
        NativeLibrary.updateFramebuffer(NativeLibrary.isPortraitMode)
    }
}
