// Copyright CitraVR Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.features.updatechecker

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.pm.PackageInstaller
import android.widget.Toast
import org.citra.citra_emu.R
import org.citra.citra_emu.utils.Log

/** Receives PackageInstaller session results committed by [UpdateInstaller]. */
class InstallStatusReceiver : BroadcastReceiver() {

    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action != ACTION_INSTALL_STATUS) {
            return
        }
        val status = intent.getIntExtra(
            PackageInstaller.EXTRA_STATUS,
            PackageInstaller.STATUS_FAILURE
        )
        when (status) {
            PackageInstaller.STATUS_PENDING_USER_ACTION -> {
                // The system wants the user to confirm the install.
                @Suppress("DEPRECATION")
                val confirmIntent = intent.getParcelableExtra<Intent>(Intent.EXTRA_INTENT)
                if (confirmIntent != null) {
                    confirmIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                    context.startActivity(confirmIntent)
                }
            }

            PackageInstaller.STATUS_SUCCESS -> {
                // For an in-place update this process is killed before getting here.
            }

            PackageInstaller.STATUS_FAILURE_ABORTED -> {
                // The user declined the confirmation dialog.
            }

            else -> {
                Log.error(
                    "[UpdateInstaller] Install failed with status $status: " +
                        intent.getStringExtra(PackageInstaller.EXTRA_STATUS_MESSAGE)
                )
                Toast.makeText(context, R.string.update_install_failed, Toast.LENGTH_LONG).show()
            }
        }
    }

    companion object {
        const val ACTION_INSTALL_STATUS = "org.citra.citra_emu.INSTALL_STATUS"
    }
}
