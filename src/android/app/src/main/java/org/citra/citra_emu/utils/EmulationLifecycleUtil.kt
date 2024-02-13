// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.utils

import org.citra.citra_emu.NativeLibrary
import org.citra.citra_emu.vr.VrActivity

object EmulationLifecycleUtil {
    private var shutdownHooks: MutableList<Runnable> = ArrayList()
    private var pauseResumeHooks: MutableList<Runnable> = ArrayList()


    fun closeGame() {
        val activity = NativeLibrary.sEmulationActivity.get()
        if (activity != null && activity is VrActivity) {
            activity.quitToMenu()
        } else {
            shutdownHooks.forEach(Runnable::run)
        }
    }

    fun pauseOrResume() {
        pauseResumeHooks.forEach(Runnable::run)
    }

    fun addShutdownHook(hook: Runnable) {
        shutdownHooks.add(hook)
    }

    fun addPauseResumeHook(hook: Runnable) {
        pauseResumeHooks.add(hook)
    }

    fun clear() {
        pauseResumeHooks.clear()
        shutdownHooks.clear()
    }
}
