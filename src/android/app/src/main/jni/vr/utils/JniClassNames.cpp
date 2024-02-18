#include "JniClassNames.h"

#include "LogUtils.h"

#include <cassert>

namespace VR {
namespace JniGlobalRef {
jmethodID gFindClassMethodID = nullptr;
jobject   gClassLoader       = nullptr;
} // namespace JniGlobalRef
} // namespace VR

void VR::JNI::InitJNI(JNIEnv* jni, jobject activityObject) {
    assert(jni != nullptr);
    const jclass activityClass = jni->GetObjectClass(activityObject);
    if (activityClass == nullptr) { FAIL("Failed to get activity class"); }

    // Get the getClassLoader method ID
    const jmethodID getClassLoaderMethod =
        jni->GetMethodID(activityClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
    if (getClassLoaderMethod == nullptr) { FAIL("Failed to get getClassLoader method ID"); }

    // Call getClassLoader of the activity object to obtain the class loader
    const jobject classLoaderObject = jni->CallObjectMethod(activityObject, getClassLoaderMethod);
    if (classLoaderObject == nullptr) { FAIL("Failed to get class loader object"); }

    JniGlobalRef::gClassLoader = jni->NewGlobalRef(classLoaderObject);

    // Step 3: Cache the findClass method ID
    jclass classLoaderClass = jni->FindClass("java/lang/ClassLoader");
    if (classLoaderClass == nullptr) { FAIL("Failed to find class loader class"); }
    JniGlobalRef::gFindClassMethodID =
        jni->GetMethodID(classLoaderClass, "findClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    if (JniGlobalRef::gFindClassMethodID == nullptr) { FAIL("Failed to get findClass method ID"); }

    // Cleanup local references
    jni->DeleteLocalRef(activityClass);
    jni->DeleteLocalRef(classLoaderClass);
}

void VR::JNI::CleanupJNI(JNIEnv* jni) {
    assert(jni != nullptr);
    if (JniGlobalRef::gClassLoader != nullptr) { jni->DeleteGlobalRef(JniGlobalRef::gClassLoader); }
    JniGlobalRef::gClassLoader       = nullptr;
    JniGlobalRef::gFindClassMethodID = nullptr;
}
