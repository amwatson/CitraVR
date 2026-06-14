// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.applets

import androidx.annotation.Keep
import java.io.Serializable
import org.citra.citra_emu.NativeLibrary
import org.citra.citra_emu.R
import org.citra.citra_emu.fragments.MiiSelectorDialogFragment
import org.citra.citra_emu.vr.VrActivity

@Keep
object MiiSelector {
    lateinit var data: MiiSelectorData
    val finishLock = Object()

    private fun executeImpl(config: MiiSelectorConfig) {
        val emulationActivity = NativeLibrary.sEmulationActivity.get()
        data = MiiSelectorData(0, 0)
        val fragment = MiiSelectorDialogFragment.newInstance(config)
        fragment.show(emulationActivity!!.supportFragmentManager, "mii_selector")
    }

    private fun vrExecuteImpl(config: MiiSelectorConfig) {
        data = MiiSelectorData(0, 0)
        val list = ArrayList<String>()
        list.add(NativeLibrary.sEmulationActivity.get()!!.getString(R.string.standard_mii))
        list.addAll(listOf<String>(*config.miiNames))
        val initialIndex =
            if (config.initiallySelectedMiiIndex < list.size) config.initiallySelectedMiiIndex as Int else 0
        data.index = initialIndex
        data.returnCode = 0
    }

    @JvmStatic
    fun execute(config: MiiSelectorConfig): MiiSelectorData {
        if (NativeLibrary.sEmulationActivity.get() is VrActivity) {
            vrExecuteImpl(config)
        } else {
            NativeLibrary.sEmulationActivity.get()!!.runOnUiThread { executeImpl(config) }
            synchronized(finishLock) {
                try {
                    finishLock.wait()
                } catch (ignored: Exception) {
                }
            }
        }
        return data
    }

    @Keep
    class MiiSelectorConfig : Serializable {
        var enableCancelButton = false
        var title: String? = null
        var initiallySelectedMiiIndex: Long = 0

        // List of Miis to display
        lateinit var miiNames: Array<String>
    }

    class MiiSelectorData(var returnCode: Long, var index: Int)
}
