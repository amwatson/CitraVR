#pragma once

#include <jni.h>

namespace VR {
namespace JniGlobalRef {
extern jmethodID gFindClassMethodID;
extern jobject   gClassLoader;

} // namespace JniGlobalRef

namespace JNI {
// Called during JNI_OnLoad
void InitJNI(JNIEnv* env, jobject activityObject);
// Called during JNI_OnUnload
void CleanupJNI(JNIEnv* env);
} // namespace JNI
} // namespace VR
