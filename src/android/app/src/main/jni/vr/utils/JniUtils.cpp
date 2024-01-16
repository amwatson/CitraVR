/*************************************************************************

Filename    :   JniUtils.cpp
Content     :   Lightweight lib for JNI.


Authors     :   Amanda M. Watson
License     :   Licensed under GPLv2 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#include "JniUtils.h"

#include "LogUtils.h"

jclass JniUtils::GetGlobalClassReference(JNIEnv* jni, jobject activityObject,
                                         const std::string& className)
{
    // First, get the class object of the activity to get its class loader
    const jclass activityClass = jni->GetObjectClass(activityObject);
    if (activityClass == nullptr)
    {
        ALOGE("Failed to get activity class");
        return nullptr;
    }

    // Get the getClassLoader method ID
    const jmethodID getClassLoaderMethod = jni->GetMethodID(
        activityClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
    if (getClassLoaderMethod == nullptr)
    {
        ALOGE("Failed to get getClassLoader method ID");
        return nullptr;
    }

    // Call getClassLoader of the activity object to obtain the class loader
    const jobject classLoaderObject =
        jni->CallObjectMethod(activityObject, getClassLoaderMethod);
    if (classLoaderObject == nullptr)
    {
        ALOGE("Failed to get class loader object");
        return nullptr;
    }

    // Get the class loader class
    const jclass classLoaderClass = jni->FindClass("java/lang/ClassLoader");
    if (classLoaderClass == nullptr)
    {
        ALOGE("Failed to get class loader class");
        return nullptr;
    }

    // Get the findClass method ID from the class loader class
    const jmethodID findClassMethod = jni->GetMethodID(
        classLoaderClass, "findClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    if (findClassMethod == nullptr)
    {
        ALOGE("Failed to get findClass method ID");
        return nullptr;
    }

    // Convert the class name string to a jstring
    const jstring javaClassName = jni->NewStringUTF(className.c_str());
    if (javaClassName == nullptr)
    {
        ALOGE("Failed to convert class name to jstring");
        return nullptr;
    }

    // Call findClass on the class loader object with the class name
    const jclass classToFind = static_cast<jclass>(jni->CallObjectMethod(
        classLoaderObject, findClassMethod, javaClassName));

    // Clean up local references
    jni->DeleteLocalRef(activityClass);
    jni->DeleteLocalRef(classLoaderObject);
    jni->DeleteLocalRef(classLoaderClass);
    jni->DeleteLocalRef(javaClassName);

    if (classToFind == nullptr)
    {
        // Handle error (Class not found)
        return nullptr;
    }

    // Create a global reference to the class
    const jclass globalClassRef =
        reinterpret_cast<jclass>(jni->NewGlobalRef(classToFind));

    // Clean up the local reference of the class
    jni->DeleteLocalRef(classToFind);

    return globalClassRef;
}
