package org.citra.citra_emu.vr;

public class VRUtils {

    // NOTE: keep this in-sync with HMDType in vr_settings.h
    public static enum HMDType {
        UNKNOWN(0), QUEST1(1), QUEST2(2), QUEST3(3), QUESTPRO(4);
        private final int value;

        HMDType(final int value) {
            this.value = value;
        }
        public int getValue() {
            return value;
        }
    }

    public static native int getHMDType();

    public static native int getDefaultResolutionFactor();
}
