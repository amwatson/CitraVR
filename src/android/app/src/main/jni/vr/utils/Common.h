/*******************************************************************************

Filename    :   Common.h

Content     :   Common utilities

Authors     :   Amanda M. Watson
License     :   Licensed under GPLv2 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#pragma once

#include <openxr/openxr.h>

#ifndef NUM_EYES
#define NUM_EYES 2 // this will never change, it just helps people know what we mean.
#endif

#define BAIL_ON_COND(cond, errorStr, returnCode)                                                   \
    do {                                                                                           \
        if (cond) {                                                                                \
            ALOGE("ERROR (%s): %s", __FUNCTION__, errorStr);                                       \
            return (returnCode);                                                                   \
        }                                                                                          \
    } while (0)

#define BAIL_ON_ERR(fn, returnCode)                                                                \
    do {                                                                                           \
        const int32_t ret = fn;                                                                    \
        if (ret < 0) {                                                                             \
            ALOGE("ERROR (%s): %s() returned %d", __FUNCTION__, #fn, ret);                         \
            return (returnCode);                                                                   \
        }                                                                                          \
    } while (0)

union XrCompositionLayer {
    XrCompositionLayerQuad mQuad;
    XrCompositionLayerCylinderKHR mCylinder;
    XrCompositionLayerPassthroughFB Passthrough;
};
