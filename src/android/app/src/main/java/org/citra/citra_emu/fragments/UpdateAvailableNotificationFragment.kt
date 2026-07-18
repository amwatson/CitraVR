// Copyright Citra Emulator Project / Azahar Emulator Project
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

class UpdateAvailableNotificationFragment(newVersionOverride: String) : DialogFragment() {
    private lateinit var mainActivity: MainActivity

    private val newVersion = newVersionOverride

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        BuildUtil.assertNotGooglePlay()
        mainActivity = requireActivity() as MainActivity

        isCancelable = false

        val updateNotificationDescription =
            getString(R.string.update_available_description, newVersion)

        return MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.update_available)
            .setMessage(updateNotificationDescription)
            .setPositiveButton(android.R.string.ok) { _: DialogInterface, _: Int ->
                val intent = Intent(
                    Intent.ACTION_VIEW,
                    Uri.parse(getString(R.string.update_link, newVersion))
                )
                startActivity(intent)
            }
            .setNegativeButton(android.R.string.cancel, null)
            .show()
    }

    companion object {
        const val TAG = "UpdateAvailableNotificationFragment"

        fun newInstance(newVersion: String): UpdateAvailableNotificationFragment {
            BuildUtil.assertNotGooglePlay()
            return UpdateAvailableNotificationFragment(newVersion)
        }
    }
}
