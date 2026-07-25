// Copyright CitraVR Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.fragments

import android.app.Dialog
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.View
import androidx.appcompat.app.AlertDialog
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.lifecycleScope
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import java.io.File
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.citra.citra_emu.R
import org.citra.citra_emu.databinding.DialogProgressBarBinding
import org.citra.citra_emu.features.updatechecker.UpdateInstaller

/**
 * Downloads the update APK with a progress bar, then hands it to the system
 * installer. If the user hasn't granted "install unknown apps" yet, walks them
 * through the grant first (the download starts automatically once they return).
 */
class UpdateInstallDialogFragment(newVersionOverride: String, apkUrlOverride: String) :
    DialogFragment() {

    private val newVersion = newVersionOverride
    private val apkUrl = apkUrlOverride

    private lateinit var binding: DialogProgressBarBinding

    private var downloadStarted = false

    @Volatile
    private var cancelled = false

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        binding = DialogProgressBarBinding.inflate(layoutInflater)
        binding.progressBar.isIndeterminate = true

        isCancelable = false
        return MaterialAlertDialogBuilder(requireContext())
            .setTitle(getString(R.string.update_installing, newVersion))
            .setView(binding.root)
            // The listener is replaced in onStart so the button doesn't dismiss the
            // dialog; it's only visible while the install permission is missing.
            .setPositiveButton(R.string.update_grant_permission, null)
            .setNegativeButton(android.R.string.cancel) { _, _ -> cancelled = true }
            .create()
    }

    override fun onStart() {
        super.onStart()
        (dialog as? AlertDialog)?.getButton(AlertDialog.BUTTON_POSITIVE)?.setOnClickListener {
            UpdateInstaller.requestInstallPermission(requireContext())
        }
    }

    override fun onResume() {
        super.onResume()
        if (!UpdateInstaller.canInstall(requireContext())) {
            binding.progressText.text = getString(R.string.update_permission_needed)
            (dialog as? AlertDialog)?.getButton(AlertDialog.BUTTON_POSITIVE)?.visibility =
                View.VISIBLE
        } else if (!downloadStarted) {
            downloadStarted = true
            (dialog as? AlertDialog)?.getButton(AlertDialog.BUTTON_POSITIVE)?.visibility =
                View.GONE
            binding.progressText.text = getString(R.string.update_downloading)
            beginDownload()
        }
    }

    private fun beginDownload() {
        val appContext = requireContext().applicationContext
        val apkFile = File(appContext.cacheDir, "update.apk")
        lifecycleScope.launch(Dispatchers.IO) {
            val downloaded = UpdateInstaller.download(
                apkUrl,
                apkFile,
                isCancelled = { cancelled }
            ) { bytesRead, totalBytes ->
                if (totalBytes > 0) {
                    launch(Dispatchers.Main) {
                        binding.progressBar.isIndeterminate = false
                        binding.progressBar.max = 100
                        binding.progressBar.progress = ((bytesRead * 100) / totalBytes).toInt()
                    }
                }
            }

            val wouldDowngrade =
                !cancelled && downloaded && UpdateInstaller.wouldDowngrade(appContext, apkFile)
            val committed = !cancelled && downloaded && !wouldDowngrade &&
                UpdateInstaller.install(appContext, apkFile)
            apkFile.delete()

            withContext(Dispatchers.Main) {
                if (cancelled) {
                    return@withContext
                }
                if (committed) {
                    // The system confirmation dialog takes over from here.
                    dismiss()
                } else if (wouldDowngrade) {
                    onFailed(getString(R.string.update_would_downgrade))
                } else {
                    onFailed(getString(R.string.update_download_failed))
                }
            }
        }
    }

    private fun onFailed(message: String) {
        binding.progressBar.visibility = View.GONE
        binding.progressText.text = message
        (dialog as? AlertDialog)?.getButton(AlertDialog.BUTTON_POSITIVE)?.let { button ->
            button.text = getString(R.string.update_open_release_page)
            button.visibility = View.VISIBLE
            button.setOnClickListener {
                startActivity(
                    Intent(
                        Intent.ACTION_VIEW,
                        Uri.parse(getString(R.string.update_link, newVersion))
                    )
                )
                dismiss()
            }
        }
    }

    companion object {
        const val TAG = "UpdateInstallDialogFragment"

        fun newInstance(newVersion: String, apkUrl: String): UpdateInstallDialogFragment =
            UpdateInstallDialogFragment(newVersion, apkUrl)
    }
}
