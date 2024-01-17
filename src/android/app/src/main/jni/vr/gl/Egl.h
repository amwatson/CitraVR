/*******************************************************************************


Filename    :   Egl.h

Authors     :   Amanda M. Watson
License     :   Licensed under GPLv2 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

class EglContext {

public:
    EglContext();
    ~EglContext();

    EGLDisplay mDisplay = 0;
    EGLConfig mConfig = 0;
    EGLContext mContext = EGL_NO_CONTEXT;

private:
    int32_t Init();
    void Shutdown();
    // Create an unused surface because our device doesn't support creating a
    // surfaceless context (KHR_surfaceless_context) (It's available in AOSP as
    // a module, though, so that might be cool).
    EGLSurface mDummySurface = EGL_NO_SURFACE;
};
