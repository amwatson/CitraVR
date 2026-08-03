// Copyright Citra Emulator Project / Azahar Emulator Project / CitraVR Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.fragments

import android.app.Dialog
import android.content.DialogInterface
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import androidx.fragment.app.DialogFragment
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.citra.citra_emu.R
import org.citra.citra_emu.ui.main.MainActivity
import org.citra.citra_emu.utils.BuildUtil

class UpdateAvailableNotificationFragment(newVersionOverride: String, apkUrlOverride: String?) :
    DialogFragment() {
    private lateinit var mainActivity: MainActivity

    private val newVersion = newVersionOverride
    private val apkUrl = apkUrlOverride

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        BuildUtil.assertNotGooglePlay()
        mainActivity = requireActivity() as MainActivity

        isCancelable = false

        val updateNotificationDescription =
            getString(R.string.update_available_description, newVersion)

        val builder = MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.update_available)
            .setMessage(updateNotificationDescription)
            .setNegativeButton(android.R.string.cancel, null)

        // Prefer the in-app installer; fall back to the browser when the release has
        // no APK asset attached.
        if (apkUrl != null) {
            builder
                .setPositiveButton(R.string.update_install) { _: DialogInterface, _: Int ->
                    UpdateInstallDialogFragment.newInstance(newVersion, apkUrl)
                        .show(mainActivity.supportFragmentManager, UpdateInstallDialogFragment.TAG)
                }
                .setNeutralButton(R.string.update_open_release_page) { _: DialogInterface, _: Int ->
                    openReleasePage()
                }
        } else {
            builder.setPositiveButton(R.string.update_open_release_page) { _, _ ->
                openReleasePage()
            }
        }

        return builder.show()
    }

    private fun openReleasePage() {
        startActivity(
            Intent(
                Intent.ACTION_VIEW,
                Uri.parse(getString(R.string.update_link, newVersion))
            )
        )
    }

    companion object {
        const val TAG = "UpdateAvailableNotificationFragment"

        fun newInstance(newVersion: String, apkUrl: String?): UpdateAvailableNotificationFragment {
            BuildUtil.assertNotGooglePlay()
            return UpdateAvailableNotificationFragment(newVersion, apkUrl)
        }
    }
}
