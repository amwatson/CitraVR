package org.citra.citra_emu.vr.utils

/** Represents a release version of Citra VR.
 * These version codes started being used in v0.4.0.
 **/
open class VrReleaseVersion(version: String) {
    private var isRealVersion: Boolean
    private var major: Int
    private var minor: Int
    private var patch: Int
    init {
        isRealVersion = isReleaseVersion(version)
        major = getVersionMajor(version)
        minor = getVersionMinor(version)
        patch = getVersionPatch(version)
    }

    fun isRealVersion() : Boolean { return isRealVersion }
    fun getMajor() : Int { return major }
    fun getMinor() : Int { return minor }
    fun getPatch() : Int { return patch }
    fun hasLowerVersionThan(versionComp: VrReleaseVersion): Boolean {
        val majorOrig = major
        val majorComp = versionComp.major
        if (majorOrig < majorComp) return true
        if (majorOrig > majorComp) return false

        val minorOrig = minor
        val minorComp = versionComp.minor
        if (minorOrig < minorComp) return true
        if (minorOrig > minorComp) return false

        val patchOrig = patch
        val patchComp = versionComp.patch
        if (patchOrig < patchComp) return true
        if (patchOrig > patchComp) return false

        return false
    }

    // Release versions are in the form "v\d+\.\d+\.\d+".
    companion object {
        val RELEASE_VERSION_0_4_0 = VrReleaseVersion("v0.4.0")
        private fun isReleaseVersion(version: String): Boolean {
            return version.startsWith("v")
        }

        private fun getVersionMajor(version: String): Int {
            return if (isReleaseVersion(version)) version.split(".")[0].removePrefix("v")
                .toInt() else -1
        }

        private fun getVersionMinor(version: String): Int {
            return if (isReleaseVersion(version)) version.split(".")[1].toInt() else -1
        }

        private fun getVersionPatch(version: String): Int {
            return if (isReleaseVersion(version)) version.split(".")[2].toInt() else -1
        }
    }

}
