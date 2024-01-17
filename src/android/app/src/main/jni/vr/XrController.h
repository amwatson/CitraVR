/*******************************************************************************


Filename    :   XrController.h
Content     :   XR-specific input handling. Specifically defines the XR
                tracked controllers and their input states, not gamepad.

Authors     :   Amanda M. Watson
License     :   Licensed under GPLv2 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#pragma once

#include <EGL/egl.h>

#include <jni.h> // needed for openxr_platform.h when XR_USE_PLATFORM_ANDROID is defined

#define XR_USE_GRAPHICS_API_OPENGL_ES 1
#define XR_USE_PLATFORM_ANDROID 1

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <memory> // for unique_ptr

class InputStateStatic {
public:
    InputStateStatic(const XrInstance& instance, const XrSession& session);
    ~InputStateStatic();
    XrActionSet mActionSet = XR_NULL_HANDLE;

    XrSpace mLeftHandSpace = XR_NULL_HANDLE;
    XrPath mLeftHandSubactionPath;

    XrAction mLeftHandIndexTriggerAction = XR_NULL_HANDLE;
    XrAction mXButtonAction = XR_NULL_HANDLE;
    XrAction mYButtonAction = XR_NULL_HANDLE;
    XrAction mLeftMenuButtonAction = XR_NULL_HANDLE;

    XrSpace mRightHandSpace = XR_NULL_HANDLE;
    XrPath mRightHandSubactionPath;

    XrAction mRightHandIndexTriggerAction = XR_NULL_HANDLE;
    XrAction mAButtonAction = XR_NULL_HANDLE;
    XrAction mBButtonAction = XR_NULL_HANDLE;

    XrAction mThumbStickAction = XR_NULL_HANDLE;
    XrAction mHandPoseAction = XR_NULL_HANDLE;
    XrAction mThumbClickAction = XR_NULL_HANDLE;
    XrAction mSqueezeTriggerAction = XR_NULL_HANDLE;
};

struct InputStateFrame {

    void SyncButtonsAndThumbSticks(const XrSession& session,
                                   const std::unique_ptr<InputStateStatic>& staticState);
    // Called after SyncButtonsAndThumbSticks
    void SyncHandPoses(const XrSession& session,
                       const std::unique_ptr<InputStateStatic>& staticState,
                       const XrSpace& referenceSpace, const XrTime predictedDisplayTime);
    enum { LEFT_CONTROLLER, RIGHT_CONTROLLER, NUM_CONTROLLERS } mPreferredHand = RIGHT_CONTROLLER;

    XrActionStateVector2f mThumbStickState[NUM_CONTROLLERS];
    XrActionStateBoolean mThumbStickClickState[NUM_CONTROLLERS];
    XrActionStateBoolean mIndexTriggerState[NUM_CONTROLLERS];
    XrActionStateBoolean mSqueezeTriggerState[NUM_CONTROLLERS];

    // Left hand buttons.
    XrActionStateBoolean mXButtonState;
    XrActionStateBoolean mYButtonState;
    XrActionStateBoolean mLeftMenuButtonState;

    // Right hand buttons.
    XrActionStateBoolean mAButtonState;
    XrActionStateBoolean mBButtonState;

    XrSpaceLocation mHandPositions[NUM_CONTROLLERS] = {{XR_TYPE_SPACE_LOCATION},
                                                       {XR_TYPE_SPACE_LOCATION}};
    bool mIsHandActive[NUM_CONTROLLERS] = {false, false};
};
