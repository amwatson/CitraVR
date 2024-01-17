/*******************************************************************************

Filename    :   LogUtils.h

Content     :   Logging macros I define in every project

Authors     :   Amanda M. Watson
License     :   Licensed under GPLv2 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#pragma once

#include <android/log.h>
#include <stdlib.h>
#ifndef LOG_TAG
#define LOG_TAG "Citra::VR"
#endif
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#ifndef NDEBUG
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#else
#define ALOGD(...)
#endif
#define FAIL(...)                                                                                  \
    do {                                                                                           \
        __android_log_print(ANDROID_LOG_FATAL, LOG_TAG, __VA_ARGS__);                              \
        abort();                                                                                   \
    } while (0)
