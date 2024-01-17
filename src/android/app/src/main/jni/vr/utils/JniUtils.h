/*************************************************************************

Filename    :   JniUtils.h
Content     :   Lightweight lib for JNI.

                If it was any less lightweight, it would
                include useful things like ref-counted objects for local
references, which are useful in complex programs where you hold the JNIEnv on
the thread for basically the entire program's duration, like this app does. That
said, this app is pretty simple, hoping I avoided any leaks.


Authors     :   Amanda M. Watson
License     :   Licensed under GPLv2 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#pragma once

#include <jni.h>

#include <string>

namespace JniUtils {
jclass GetGlobalClassReference(JNIEnv* jni, jobject activityObject, const std::string& className);
} // namespace JniUtils
