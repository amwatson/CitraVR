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

    // TODO: Consider how cycling should handle custom layout
    // right now it simply skips it
    fun cycleLayouts() {
        val nextLayout = if (NativeLibrary.isPortraitMode) {
            (EmulationMenuSettings.portraitScreenLayout + 1) % (PortraitScreenLayout.entries.size - 1)
        } else {
            (EmulationMenuSettings.landscapeScreenLayout + 1) % (ScreenLayout.entries.size - 1)
        }
        settings.loadSettings()

        changeScreenOrientation(nextLayout)
    }

    fun changePortraitOrientation(layoutOption: Int) {
        EmulationMenuSettings.portraitScreenLayout = layoutOption
        NativeLibrary.notifyPortraitLayoutChange(
            EmulationMenuSettings.portraitScreenLayout,
            windowManager.defaultDisplay.rotation,
            NativeLibrary::isPortraitMode.get()
        )
        IntSetting.PORTRAIT_SCREEN_LAYOUT.int = layoutOption
        settings.saveSetting(IntSetting.PORTRAIT_SCREEN_LAYOUT, SettingsFile.FILE_NAME_CONFIG)
    }

    fun changeScreenOrientation(layoutOption: Int) {
        EmulationMenuSettings.landscapeScreenLayout = layoutOption
        NativeLibrary.notifyOrientationChange(
            EmulationMenuSettings.landscapeScreenLayout,
            windowManager.defaultDisplay.rotation,
            NativeLibrary::isPortraitMode.get()
        )
        IntSetting.SCREEN_LAYOUT.int = layoutOption
        settings.saveSetting(IntSetting.SCREEN_LAYOUT, SettingsFile.FILE_NAME_CONFIG)

    }
}
