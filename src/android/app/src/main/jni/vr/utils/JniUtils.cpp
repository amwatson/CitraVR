/*************************************************************************

Filename    :   JniUtils.cpp
Content     :   Lightweight lib for JNI.


Authors     :   Amanda M. Watson
License     :   Licensed under GPLv3 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#include "JniUtils.h"

#include "JniClassNames.h"
#include "LogUtils.h"

jclass JniUtils::GetGlobalClassReference(JNIEnv* env, jobject activityObject,
                                         const std::string& className) {
    // Convert dot ('.') to slash ('/') in class name (Java uses dots, JNI uses slashes for class
    // names)
    std::string correctedClassName = className;
    std::replace(correctedClassName.begin(), correctedClassName.end(), '.', '/');

    // Convert std::string to jstring
    jstring classNameJString = env->NewStringUTF(correctedClassName.c_str());

    // Use the global class loader to find the class
    jclass clazz = static_cast<jclass>(env->CallObjectMethod(
        VR::JniGlobalRef::gClassLoader, VR::JniGlobalRef::gFindClassMethodID, classNameJString));
    if (clazz == nullptr) {
        // Class not found
        ALOGE("Class not found: %s", correctedClassName.c_str());
        return nullptr;
    }

    // Clean up the local reference to the class name jstring
    env->DeleteLocalRef(classNameJString);

    // Check for exceptions and handle them. This is crucial to prevent crashes due to uncaught
    // exceptions.
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        return nullptr; // Class not found or other issue
    }

    // Return a global reference to the class
    return static_cast<jclass>(env->NewGlobalRef(clazz));
}
