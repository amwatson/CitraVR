package org.citra.citra_emu.vr

object VRUtils {
    val hMDType: Int
        external get
    val defaultResolutionFactor: Int
        external get

    // NOTE: keep this in-sync with HMDType in vr_settings.h
    enum class HMDType(val value: Int) {
        UNKNOWN(0),
        QUEST1(1),
        QUEST2(2),
        QUEST3(3),
        QUESTPRO(4)

    }
}
