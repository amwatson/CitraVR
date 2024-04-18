#include "JniClassNames.h"

#include "LogUtils.h"

#include <cassert>

namespace VR {
namespace JniGlobalRef {
jclass gVrKeyboardLayerClass     = nullptr;
jclass gVrErrorMessageLayerClass = nullptr;
jclass gVrRibbonLayerClass       = nullptr;
} // namespace JniGlobalRef
} // namespace VR

void VR::JNI::InitJNI(JNIEnv* jni) {
    assert(jni != nullptr);

    VR::JniGlobalRef::gVrKeyboardLayerClass = static_cast<jclass>(
        jni->NewGlobalRef(jni->FindClass("org/citra/citra_emu/vr/ui/VrKeyboardLayer")));
    if (VR::JniGlobalRef::gVrKeyboardLayerClass == nullptr) {
        FAIL("Could not find VrKeyboardLayer class");
    }
    VR::JniGlobalRef::gVrErrorMessageLayerClass = static_cast<jclass>(
        jni->NewGlobalRef(jni->FindClass("org/citra/citra_emu/vr/ui/VrErrorMessageLayer")));
    if (VR::JniGlobalRef::gVrErrorMessageLayerClass == nullptr) {
        FAIL("Could not find VrErrorMessageLayer class");
    }

    VR::JniGlobalRef::gVrRibbonLayerClass = static_cast<jclass>(
        jni->NewGlobalRef(jni->FindClass("org/citra/citra_emu/vr/ui/VrRibbonLayer")));
    if (VR::JniGlobalRef::gVrRibbonLayerClass == nullptr) {
        FAIL("Could not find VrRibbonLayer class");
    }
}

void VR::JNI::CleanupJNI(JNIEnv* jni) {
    assert(jni != nullptr);
    VR::JniGlobalRef::gVrKeyboardLayerClass     = nullptr;
    VR::JniGlobalRef::gVrErrorMessageLayerClass = nullptr;
    VR::JniGlobalRef::gVrRibbonLayerClass       = nullptr;
}
