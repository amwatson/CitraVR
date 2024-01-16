/*******************************************************************************


Filename    :   Egl.cpp

Authors     :   Amanda M. Watson
License     :   Licensed under GPLv2 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#include "Egl.h"
#include "../utils/LogUtils.h"

namespace
{

const char* EglErrorToStr(const EGLint error)
{
    switch (error)
    {
        case EGL_SUCCESS:
            return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED:
            return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS:
            return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC:
            return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE:
            return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONTEXT:
            return "EGL_BAD_CONTEXT";
        case EGL_BAD_CONFIG:
            return "EGL_BAD_CONFIG";
        case EGL_BAD_CURRENT_SURFACE:
            return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY:
            return "EGL_BAD_DISPLAY";
        case EGL_BAD_SURFACE:
            return "EGL_BAD_SURFACE";
        case EGL_BAD_MATCH:
            return "EGL_BAD_MATCH";
        case EGL_BAD_PARAMETER:
            return "EGL_BAD_PARAMETER";
        case EGL_BAD_NATIVE_PIXMAP:
            return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW:
            return "EGL_BAD_NATIVE_WINDOW";
        case EGL_CONTEXT_LOST:
            return "EGL_CONTEXT_LOST";
        default:
            return "UNKNOWN";
    }
}
} // anonymous namespace

EglContext::EglContext()
{

    const int32_t ret = Init();
    if (ret < 0) { FAIL("EglContext::EglContext() failed: ret=%i", ret); }
}

EglContext::~EglContext() { Shutdown(); }

// next return code: -7
int32_t EglContext::Init()
{
    mDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (mDisplay == EGL_NO_DISPLAY)
    {
        ALOGE("        eglGetDisplay() failed: %s",
              EglErrorToStr(eglGetError()));
        return -1;
    }
    {
        ALOGV("        eglInitialize(mDisplay, &MajorVersion, &MinorVersion)");
        EGLint majorVersion = 0;
        EGLint minorVersion = 0;
        if (eglInitialize(mDisplay, &majorVersion, &minorVersion) == EGL_FALSE)
        {
            ALOGE("        eglInitialize() failed: %s",
                  EglErrorToStr(eglGetError()));
            return -2;
        }
    }
    EGLint       numConfigs      = 0;
    const EGLint configAttribs[] = {
        EGL_RED_SIZE,
        8,
        EGL_GREEN_SIZE,
        8,
        EGL_BLUE_SIZE,
        8,
        EGL_ALPHA_SIZE,
        8,
        EGL_DEPTH_SIZE, // probably don't need
        0,
        EGL_STENCIL_SIZE, // probably don't need, but we'll see.
        0,
        EGL_SAMPLES,
        0,
        EGL_NONE};

    if (eglChooseConfig(mDisplay, configAttribs, &mConfig, 1, &numConfigs) ==
        EGL_FALSE)
    {
        ALOGE("        eglChooseConfig() failed: %s",
              EglErrorToStr(eglGetError()));
        return -3;
    }
    // print out chosen config attributes
    ALOGV("        Chosen EGLConfig attributes:");
    {
        EGLint value = 0;
        eglGetConfigAttrib(mDisplay, mConfig, EGL_RED_SIZE, &value);
        ALOGV("            EGL_RED_SIZE: %i", value);
    }
    {
        EGLint value = 0;
        eglGetConfigAttrib(mDisplay, mConfig, EGL_GREEN_SIZE, &value);
        ALOGV("            EGL_GREEN_SIZE: %i", value);
    }
    {
        EGLint value = 0;
        eglGetConfigAttrib(mDisplay, mConfig, EGL_BLUE_SIZE, &value);
        ALOGV("            EGL_BLUE_SIZE: %i", value);
    }
    {
        EGLint value = 0;
        eglGetConfigAttrib(mDisplay, mConfig, EGL_ALPHA_SIZE, &value);
        ALOGV("            EGL_ALPHA_SIZE: %i", value);
    }
    {
        EGLint value = 0;
        eglGetConfigAttrib(mDisplay, mConfig, EGL_DEPTH_SIZE, &value);
        ALOGV("            EGL_DEPTH_SIZE: %i", value);
    }
    {
        EGLint value = 0;
        eglGetConfigAttrib(mDisplay, mConfig, EGL_STENCIL_SIZE, &value);
        ALOGV("            EGL_STENCIL_SIZE: %i", value);
    }
    {
        EGLint value = 0;
        eglGetConfigAttrib(mDisplay, mConfig, EGL_SAMPLES, &value);
        ALOGV("            EGL_SAMPLES: %i", value);
    }

    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    ALOGV("        mContext = eglCreateContext(mDisplay, mConfig, "
          "EGL_NO_CONTEXT, "
          "contextAttribs)");
    mContext =
        eglCreateContext(mDisplay, mConfig, EGL_NO_CONTEXT, contextAttribs);
    if (mContext == EGL_NO_CONTEXT)
    {
        ALOGE("        eglCreateContext() failed: %s",
              EglErrorToStr(eglGetError()));
        return -4;
    }
    const EGLint surfaceAttribs[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
    ALOGV("        mDummySurface = eglCreatePbufferSurface(mDisplay, mConfig, "
          "surfaceAttribs)");
    mDummySurface = eglCreatePbufferSurface(mDisplay, mConfig, surfaceAttribs);
    if (mDummySurface == EGL_NO_SURFACE)
    {
        ALOGE("        eglCreatePbufferSurface() failed: %s",
              EglErrorToStr(eglGetError()));
        eglDestroyContext(mDisplay, mContext);
        mContext = EGL_NO_CONTEXT;
        return -5;
    }
    ALOGV("        eglMakeCurrent(mDisplay, mDummySurface, mDummySurface, "
          "mContext)");
    if (eglMakeCurrent(mDisplay, mDummySurface, mDummySurface, mContext) ==
        EGL_FALSE)
    {
        ALOGE("        eglMakeCurrent() failed: %s",
              EglErrorToStr(eglGetError()));
        eglDestroySurface(mDisplay, mDummySurface);
        eglDestroyContext(mDisplay, mContext);
        mContext = EGL_NO_CONTEXT;
        return -6;
    }

    return 0;
}

void EglContext::Shutdown()
{
    if (mContext != EGL_NO_CONTEXT)
    {
        ALOGV(
            "        eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, "
            "EGL_NO_CONTEXT)");
        eglMakeCurrent(
            mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        ALOGV("        eglDestroySurface(mDisplay, mDummySurface)");
        eglDestroySurface(mDisplay, mDummySurface);
        ALOGV("        eglDestroyContext(mDisplay, mContext)");
        eglDestroyContext(mDisplay, mContext);
        mContext = EGL_NO_CONTEXT;
    }
    if (mDisplay != EGL_NO_DISPLAY)
    {
        ALOGV("        eglTerminate(mDisplay)");
        eglTerminate(mDisplay);
        mDisplay = EGL_NO_DISPLAY;
    }
}
