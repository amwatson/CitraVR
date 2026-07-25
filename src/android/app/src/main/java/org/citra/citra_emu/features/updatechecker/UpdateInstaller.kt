// Copyright CitraVR Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.features.updatechecker

import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.PackageInstaller
import android.net.Uri
import android.provider.Settings
import java.io.File
import java.net.HttpURLConnection
import java.net.URL
import kotlin.time.Duration.Companion.seconds
import org.citra.citra_emu.utils.Log

/**
 * Downloads a release APK and hands it to the system PackageInstaller, which
 * verifies the signature against the installed app and shows the install
 * confirmation dialog. The commit result arrives via [InstallStatusReceiver].
 */
object UpdateInstaller {

    fun canInstall(context: Context): Boolean = context.packageManager.canRequestPackageInstalls()

    /** Opens the system page where the user grants CitraVR the "install unknown apps" toggle. */
    fun requestInstallPermission(context: Context) {
        context.startActivity(
            Intent(
                Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
                Uri.parse("package:${context.packageName}")
            )
        )
    }

    /**
     * Streams [urlString] into [destination], reporting (bytesRead, totalBytes) via
     * [onProgress] (totalBytes is -1 when unknown). Returns false on any failure or
     * when [isCancelled] starts returning true.
     */
    fun download(
        urlString: String,
        destination: File,
        isCancelled: () -> Boolean,
        onProgress: (Long, Long) -> Unit
    ): Boolean {
        var connection: HttpURLConnection? = null
        try {
            connection = URL(urlString).openConnection() as HttpURLConnection
            connection.connectTimeout = 15.seconds.inWholeMilliseconds.toInt()
            connection.readTimeout = 15.seconds.inWholeMilliseconds.toInt()

            if (connection.responseCode != HttpURLConnection.HTTP_OK) {
                Log.error(
                    "[UpdateInstaller] Download failed with HTTP response code ${connection.responseCode}"
                )
                return false
            }

            val totalBytes = connection.contentLengthLong
            var bytesRead = 0L
            connection.inputStream.use { input ->
                destination.outputStream().use { output ->
                    val buffer = ByteArray(64 * 1024)
                    while (true) {
                        if (isCancelled()) {
                            return false
                        }
                        val read = input.read(buffer)
                        if (read < 0) {
                            break
                        }
                        output.write(buffer, 0, read)
                        bytesRead += read
                        onProgress(bytesRead, totalBytes)
                    }
                }
            }
            return true
        } catch (e: Exception) {
            Log.error("[UpdateInstaller] Download failed: $e: ${e.message}")
            return false
        } finally {
            connection?.disconnect()
        }
    }

    /**
     * True when installing [apk] over this app would be rejected by Android as a
     * version downgrade — i.e. the running build was compiled after the downloaded
     * release (versionCode is a build timestamp). Only applies when the APK has the
     * same package name; installing a different package is a fresh install with no
     * downgrade rule.
     */
    fun wouldDowngrade(context: Context, apk: File): Boolean {
        val archiveInfo =
            context.packageManager.getPackageArchiveInfo(apk.path, 0) ?: return false
        if (archiveInfo.packageName != context.packageName) {
            return false
        }
        val installedInfo = context.packageManager.getPackageInfo(context.packageName, 0)
        return archiveInfo.longVersionCode <= installedInfo.longVersionCode
    }

    /**
     * Commits [apk] through a PackageInstaller session. Returns false if the session
     * couldn't be created or written; the actual install outcome (including the
     * user's confirmation) is delivered to [InstallStatusReceiver] afterward.
     */
    fun install(context: Context, apk: File): Boolean {
        try {
            val installer = context.packageManager.packageInstaller
            val params = PackageInstaller.SessionParams(
                PackageInstaller.SessionParams.MODE_FULL_INSTALL
            )
            val sessionId = installer.createSession(params)
            installer.openSession(sessionId).use { session ->
                session.openWrite("update.apk", 0, apk.length()).use { output ->
                    apk.inputStream().use { it.copyTo(output) }
                    session.fsync(output)
                }
                val statusIntent = Intent(context, InstallStatusReceiver::class.java)
                    .setAction(InstallStatusReceiver.ACTION_INSTALL_STATUS)
                val pendingIntent = PendingIntent.getBroadcast(
                    context,
                    sessionId,
                    statusIntent,
                    PendingIntent.FLAG_MUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
                )
                session.commit(pendingIntent.intentSender)
            }
            return true
        } catch (e: Exception) {
            Log.error("[UpdateInstaller] Install session failed: $e: ${e.message}")
            return false
        }
    }
}
