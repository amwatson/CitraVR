/*******************************************************************************

Filename    :   OpenXR.h

Content     :   OpenXR initialization and shutdown code.

Authors     :   Amanda M. Watson
License     :   Licensed under GPLv2 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#pragma once
#include "gl/Egl.h"

#include <jni.h>

#define XR_USE_GRAPHICS_API_OPENGL_ES 1
#define XR_USE_PLATFORM_ANDROID       1

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <memory> // for unique_ptr

#define OXR(func) OXR_CheckErrors(func, #func, true);
void OXR_CheckErrors(XrResult result, const char* function, bool failOnError);

class OpenXr
{

public:
    static XrInstance& GetInstance();
    int32_t            Init(JavaVM* const jvm, const jobject activityObject);
    void               Shutdown();

    XrInstance instance_ = XR_NULL_HANDLE;
    XrSystemId systemId_ = XR_NULL_SYSTEM_ID;
    XrSession  session_  = XR_NULL_HANDLE;
    // this structure contains the HMD's recommended eye textures sizes. In
    // OpenXR, this is meant to be extensible to allow spectator views as well.
    // One for each eye.
    XrViewConfigurationView                  viewConfigurationViews_[2] = {};
    XrViewConfigurationProperties            viewportConfig_            = {};
    static constexpr XrViewConfigurationType VIEW_CONFIG_TYPE =
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    XrSpace headSpace_             = XR_NULL_HANDLE;
    XrSpace forwardDirectionSpace_ = XR_NULL_HANDLE;

    XrSpace localSpace_    = XR_NULL_HANDLE;
    XrSpace stageSpace_    = XR_NULL_HANDLE;
    size_t  maxLayerCount_ = 0;

    // EGL context
    std::unique_ptr<EglContext> eglContext_;

private:
    int32_t OpenXRInit(JavaVM* const jvm, const jobject activityObject);
    int32_t XrViewConfigInit();
    int32_t XrSpaceInit();
    void    XrSpaceDestroy();
};
