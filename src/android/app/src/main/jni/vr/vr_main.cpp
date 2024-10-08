/*******************************************************************************

Filename    :   vr_main.cpp
Content     :   VR entrypoint for Android. Called from onCreate to iniitalize
                the "VrApp" thread, which handles OpenXR and XR-specific
                rendering.
Authors     :   Amanda M. Watson
License     :   Licensed under GPLv3 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#include "OpenXR.h"
#include "utils/LogUtils.h"

#include "XrController.h"
#include "layers/CursorLayer.h"
#include "layers/GameSurfaceLayer.h"
#include "layers/PassthroughLayer.h"
#include "layers/RibbonLayer.h"
#include "layers/UILayer.h"
#include "vr_settings.h"

#include "utils/Common.h"
#include "utils/JniClassNames.h"
#include "utils/MessageQueue.h"
#include "utils/SyspropUtils.h"
#include "utils/XrMath.h"

#include <android/native_window_jni.h>
#include <jni.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <assert.h>
#include <sys/prctl.h>
#include <unistd.h>

#include "core/core.h"
#include "video_core/gpu.h"
#include "video_core/renderer_base.h"

#if defined(DEBUG_INPUT_VERBOSE)
#define ALOG_INPUT_VERBOSE(...) ALOGI(__VA_ARGS__)
#else
#define ALOG_INPUT_VERBOSE(...)
#endif

// Used by Citra core to set a higher priority to the non-VR render thread.
namespace vr {
XrSession  gSession     = XR_NULL_HANDLE;
int        gPriorityTid = -1;
bool       gCitraReady  = false;
XrSession& GetSession() { return gSession; }
void       PrioritizeTid(const int tid) {
          assert(gSession != XR_NULL_HANDLE);
          if (gSession == XR_NULL_HANDLE) {
              ALOGE("PrioritizeTid() called before session is initialized");
              return;
    }
          PFN_xrSetAndroidApplicationThreadKHR pfnSetAndroidApplicationThreadKHR = NULL;
          OXR(xrGetInstanceProcAddr(OpenXr::GetInstance(), "xrSetAndroidApplicationThreadKHR",
                                    (PFN_xrVoidFunction*)(&pfnSetAndroidApplicationThreadKHR)));

          OXR(pfnSetAndroidApplicationThreadKHR(gSession, XR_ANDROID_THREAD_TYPE_RENDERER_MAIN_KHR, tid));
          gPriorityTid = tid;
          ALOGD("Setting prio tid from original code {}", vr::gPriorityTid);
}

void SetCitraReady() { gCitraReady = true; }
} // namespace vr

namespace {
constexpr XrPerfSettingsLevelEXT                   kGpuPerfLevel = XR_PERF_SETTINGS_LEVEL_BOOST_EXT;
std::chrono::time_point<std::chrono::steady_clock> gOnCreateStartTime;
std::unique_ptr<OpenXr>                            gOpenXr;
MessageQueue                                       gMessageQueue;

const std::vector<float> immersiveScaleFactor = {1.0f, 3.0f, 1.4f};

void ForwardButtonStateChangeToCitra(JNIEnv* jni, jobject activityObject,
                                     jmethodID forwardVRInputMethodID, const int androidButtonCode,
                                     const XrBool32 xrButtonState) {
    jni->CallVoidMethod(activityObject, forwardVRInputMethodID, androidButtonCode,
                        xrButtonState != XR_FALSE);
}

bool ShouldForwardButtonState(const XrActionStateBoolean& actionState) {
    return actionState.changedSinceLastSync || actionState.currentState == XR_TRUE;
}

void ForwardButtonStateIfNeeded(JNIEnv* jni, jobject activityObject,
                                jmethodID forwardVRInputMethodID, const int androidButtonCode,
                                const XrActionStateBoolean& actionState, const char* buttonName) {
    if (ShouldForwardButtonState(actionState)) {
        ALOG_INPUT_VERBOSE("Forwarding {} button state: {}", buttonName, actionState.currentState);
        ForwardButtonStateChangeToCitra(jni, activityObject, forwardVRInputMethodID,
                                        androidButtonCode, actionState.currentState);
    }
}

void SendTriggerStateToWindow(JNIEnv* jni, jobject activityObject,
                              jmethodID                   sendClickToWindowMethodID,
                              const XrActionStateBoolean& triggerState,
                              const XrVector2f&           cursorPos2d) {
    if (triggerState.currentState == 0 && triggerState.changedSinceLastSync) {
        jni->CallVoidMethod(activityObject, sendClickToWindowMethodID, cursorPos2d.x, cursorPos2d.y,
                            0);
    } else if (triggerState.changedSinceLastSync && triggerState.currentState == 1) {
        jni->CallVoidMethod(activityObject, sendClickToWindowMethodID, cursorPos2d.x, cursorPos2d.y,
                            1);
    } else if (triggerState.currentState == 1 && !triggerState.changedSinceLastSync) {
        jni->CallVoidMethod(activityObject, sendClickToWindowMethodID, cursorPos2d.x, cursorPos2d.y,
                            2);
    }
}

[[maybe_unused]] const char* XrSessionStateToString(const XrSessionState state) {
    switch (state) {
        case XR_SESSION_STATE_UNKNOWN:
            return "XR_SESSION_STATE_UNKNOWN";
        case XR_SESSION_STATE_IDLE:
            return "XR_SESSION_STATE_IDLE";
        case XR_SESSION_STATE_READY:
            return "XR_SESSION_STATE_READY";
        case XR_SESSION_STATE_SYNCHRONIZED:
            return "XR_SESSION_STATE_SYNCHRONIZED";
        case XR_SESSION_STATE_VISIBLE:
            return "XR_SESSION_STATE_VISIBLE";
        case XR_SESSION_STATE_FOCUSED:
            return "XR_SESSION_STATE_FOCUSED";
        case XR_SESSION_STATE_STOPPING:
            return "XR_SESSION_STATE_STOPPING";
        case XR_SESSION_STATE_LOSS_PENDING:
            return "XR_SESSION_STATE_LOSS_PENDING";
        case XR_SESSION_STATE_EXITING:
            return "XR_SESSION_STATE_EXITING";
        case XR_SESSION_STATE_MAX_ENUM:
            return "XR_SESSION_STATE_MAX_ENUM";
        default:
            return "Unknown";
    }
}

uint32_t GetDefaultGameResolutionFactorForHmd(const VRSettings::HMDType& hmdType) {
    static constexpr uint32_t kDefaultResolutionFactor = 2;
    switch (hmdType) {
        case VRSettings::HMDType::QUEST3:
            return 3;
        case VRSettings::HMDType::UNKNOWN:
            ALOGW("Warning: Unknown HMD type, using default scale factor of {}",
                  kDefaultResolutionFactor);
        case VRSettings::HMDType::QUEST1:
            ALOGW("Warning: Unsupported HMD type, using default scale factor "
                  "of {}",
                  kDefaultResolutionFactor);
        case VRSettings::HMDType::QUEST2:
        case VRSettings::HMDType::QUESTPRO:
        case VRSettings::HMDType::QUEST3S:
            return kDefaultResolutionFactor;
    }
}

// Called whenever a session is started/resumed. Creates the head space based on the
// current pose of the HMD.
void CreateRuntimeInitatedReferenceSpaces(const XrTime predictedDisplayTime) {
    // Create a reference space with the forward direction from the
    // starting frame.
    {
        const XrReferenceSpaceCreateInfo sci = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO, nullptr,
                                                XR_REFERENCE_SPACE_TYPE_LOCAL,
                                                XrMath::Posef::Identity()};
        OXR(xrCreateReferenceSpace(gOpenXr->mSession, &sci, &gOpenXr->mForwardDirectionSpace));
    }

    {
        const XrReferenceSpaceCreateInfo sci = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO, nullptr,
                                                XR_REFERENCE_SPACE_TYPE_VIEW,
                                                XrMath::Posef::Identity()};
        OXR(xrCreateReferenceSpace(gOpenXr->mSession, &sci, &gOpenXr->mViewSpace));
    }

    // Get the pose of the local space.
    XrSpaceLocation lsl = {XR_TYPE_SPACE_LOCATION};
    OXR(xrLocateSpace(gOpenXr->mForwardDirectionSpace, gOpenXr->mLocalSpace, predictedDisplayTime,
                      &lsl));

    // Set the forward direction of the new space.
    const XrPosef forwardDirectionPose = lsl.pose;

    // Create a reference space with the same position and rotation as
    // local.
    const XrReferenceSpaceCreateInfo sci = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO, nullptr,
                                            XR_REFERENCE_SPACE_TYPE_LOCAL, forwardDirectionPose};
    OXR(xrCreateReferenceSpace(gOpenXr->mSession, &sci, &gOpenXr->mHeadSpace));
}

} // anonymous namespace

//-----------------------------------------------------------------------------
// VRApp

class VRApp {
public:
    VRApp(jobject activityObjectGlobalRef)
        : mActivityObject(activityObjectGlobalRef) {}

    ~VRApp() { assert(mLastAppState.mIsStopRequested); }

private:
    class AppState;

public:
    void MainLoop(JNIEnv* jni) {

        //////////////////////////////////////////////////
        // Init
        //////////////////////////////////////////////////

        Init(jni);

        //////////////////////////////////////////////////
        // Frame loop
        //////////////////////////////////////////////////

        while (true) {
            // Handle events/state-changes.
            AppState appState = HandleEvents(jni);
            if (appState.mIsStopRequested) { break; }
            HandleStateChanges(jni, appState);
            if (appState.mIsXrSessionActive) {
                // Increment the frame index.
                // Frame index starts at 1. I don't know why, we've always done this.
                // Doesn't actually matter, except to make the indices
                // consistent in traces
                mFrameIndex++;
                // Log time to first frame
                if (mFrameIndex == 1) {
                    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
                    ALOGI("Time to first frame: {} ms",
                          std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                                                gOnCreateStartTime)
                              .count());
                }

                // Update non-tracking-dependent-state.
                mInputStateFrame.SyncButtonsAndThumbSticks(gOpenXr->mSession, mInputStateStatic);
                HandleInput(jni, mInputStateFrame, appState);

                Frame(jni, appState);
            } else {
                // FIXME: currently, the way this is set up, some messages can be discarded if they
                // aren't processed on the next frame.
                // This currently requires that all AppState state-related events be handled in
                // HandleStateChanges. and not in the frame for 100% correctness (consequence is
                // losing messages on unmount). This is possibly solved by handling MessageQueue
                // events inside the frame call.
                // TODO should block here
                mFrameIndex = 0;
            }
            mLastAppState = appState;
        }

        //////////////////////////////////////////////////
        // Exit
        //////////////////////////////////////////////////

        ALOGI("::MainLoop() exiting");
    }

private:
    void Init(JNIEnv* jni) {
        assert(gOpenXr != nullptr);
        mInputStateStatic =
            std::make_unique<InputStateStatic>(OpenXr::GetInstance(), gOpenXr->mSession);

        //////////////////////////////////////////////////
        // Create the layers
        //////////////////////////////////////////////////

        // Create the background layer.
        assert(VRSettings::values.vr_environment ==
                   static_cast<int32_t>(VRSettings::VREnvironmentType::VOID) ||
               VRSettings::values.vr_environment ==
                   static_cast<int32_t>(VRSettings::VREnvironmentType::PASSTHROUGH));
        // If user set "Void" in settings, don't render passthrough
        if (VRSettings::values.vr_environment !=
            static_cast<int32_t>(VRSettings::VREnvironmentType::VOID)) {
            mPassthroughLayer = std::make_unique<PassthroughLayer>(gOpenXr->mSession);
        }

        // Create the game surface layer.
        {
            ALOGI("VR Extra Performance Mode: {}",
                  VRSettings::values.extra_performance_mode_enabled ? "enabled" : "disabled");

            const uint32_t defaultResolutionFactor =
                GetDefaultGameResolutionFactorForHmd(VRSettings::values.hmd_type);
            const uint32_t resolutionFactorFromPreferences = VRSettings::values.resolution_factor;
            // add a couple factors to resolution with immersive mode so users
            // aren't resetting their default settings to get higher res. min
            // resolution factor for immersive is 3x.
            const uint32_t immersiveModeOffset = (VRSettings::values.vr_immersive_mode > 0) ? 2 : 0;
            const uint32_t resolutionFactor =
                (resolutionFactorFromPreferences > 0 ? resolutionFactorFromPreferences
                                                     : defaultResolutionFactor) +
                immersiveModeOffset;

            if (resolutionFactor != defaultResolutionFactor) {
                ALOGI("Using resolution factor of {}x instead of HMD default {}x", resolutionFactor,
                      defaultResolutionFactor);
            }
            mGameSurfaceLayer = std::make_unique<GameSurfaceLayer>(
                XrVector3f{0, 0, VRSettings::values.vr_immersive_mode > 0 ? -0.5f : -2.0f}, jni,
                mActivityObject, gOpenXr->mSession, resolutionFactor);
        }

        mRibbonLayer = std::make_unique<RibbonLayer>(mGameSurfaceLayer->GetLowerPanelPose(), jni,
                                                     mActivityObject, gOpenXr->mSession);

        mKeyboardLayer = std::make_unique<UILayer>(
            VR::JniGlobalRef::gVrKeyboardLayerClass, XrVector3f{0, -0.4f, -0.5f},
            XrMath::Quatf::FromEuler(-MATH_FLOAT_PI / 4.0f, 0.0f, 0.0f), jni, mActivityObject,
            gOpenXr->mSession);

        mErrorMessageLayer = std::make_unique<UILayer>(
            VR::JniGlobalRef::gVrErrorMessageLayerClass, XrVector3f{0, -0.1f, -1.0f},
            XrQuaternionf{0, 0, 0, 1}, jni, mActivityObject, gOpenXr->mSession);

        // Create the cursor layer.
        mCursorLayer = std::make_unique<CursorLayer>(gOpenXr->mSession);

        //////////////////////////////////////////////////
        // Intialize JNI methods
        //////////////////////////////////////////////////

        mForwardVRInputMethodID =
            jni->GetMethodID(jni->GetObjectClass(mActivityObject), "forwardVRInput", "(IZ)V");
        if (mForwardVRInputMethodID == nullptr) { FAIL("could not get forwardVRInputMethodID"); }
        mForwardVRJoystickMethodID =
            jni->GetMethodID(jni->GetObjectClass(mActivityObject), "forwardVRJoystick", "(FFI)V");
        if (mForwardVRJoystickMethodID == nullptr) {
            FAIL("could not get forwardVRJoystickMethodID");
        }

        mSendClickToWindowMethodID =
            jni->GetMethodID(jni->GetObjectClass(mActivityObject), "sendClickToWindow", "(FFI)V");
        if (mSendClickToWindowMethodID == nullptr) {
            FAIL("could not get sendClickToWindowMethodID");
        }
        mResumeGameMethodID =
            jni->GetMethodID(jni->GetObjectClass(mActivityObject), "resumeGame", "()V");
        if (mResumeGameMethodID == nullptr) { FAIL("could not get resumeGameMethodID"); }
        mPauseGameMethodID =
            jni->GetMethodID(jni->GetObjectClass(mActivityObject), "pauseGame", "()V");
        if (mPauseGameMethodID == nullptr) { FAIL("could not get pauseGameMethodID"); }
        mOpenSettingsMethodID =
            jni->GetMethodID(jni->GetObjectClass(mActivityObject), "openSettingsMenu", "()V");
        if (mOpenSettingsMethodID == nullptr) { FAIL("could not get openSettingsMenuMethodID"); }

        if (VRSettings::values.vr_immersive_mode != 0) {
            mLastAppState.mIsLowerMenuToggledOn = false;
        }
    }

    void Frame(JNIEnv* jni, const AppState& appState) {

        ////////////////////////////////
        // XrWaitFrame()
        ////////////////////////////////

        // XrWaitFrame returns the predicted display time.
        XrFrameState frameState = {XR_TYPE_FRAME_STATE, nullptr};

        {
            XrFrameWaitInfo wfi = {XR_TYPE_FRAME_WAIT_INFO, nullptr};
            OXR(xrWaitFrame(gOpenXr->mSession, &wfi, &frameState));
        }

        ////////////////////////////////
        // XrBeginFrame()
        ////////////////////////////////

        {
            XrFrameBeginInfo bfd = {XR_TYPE_FRAME_BEGIN_INFO, nullptr};
            OXR(xrBeginFrame(gOpenXr->mSession, &bfd));
        }

        ///////////////////////////////////////////////////
        // Get tracking, space, projection info for frame.
        ///////////////////////////////////////////////////

        // Re-initialize the reference spaces on the first frame so
        // it is in-sync with user
        if (mFrameIndex == 1) {
            CreateRuntimeInitatedReferenceSpaces(frameState.predictedDisplayTime);
        }

        gOpenXr->headLocation = {XR_TYPE_SPACE_LOCATION};
        OXR(xrLocateSpace(gOpenXr->mViewSpace, gOpenXr->mHeadSpace, frameState.predictedDisplayTime,
                          &gOpenXr->headLocation));

        mInputStateFrame.SyncHandPoses(gOpenXr->mSession, mInputStateStatic, gOpenXr->mLocalSpace,
                                       frameState.predictedDisplayTime);

        ///////////////////////////////////////////////////
        // Super Immersive Mode update and computation.
        //////////////////////////////////////////////////

        // enable toggle when menu is set to main. Otherwise, always on (super immersive disabled).
        const bool showUIRibbon = appState.mLowerMenuType == LowerMenuType::POSITIONAL_MENU ||
                                  appState.mIsLowerMenuToggledOn;

        float      immersiveModeFactor    = (VRSettings::values.vr_immersive_mode < 2)
                                                ? immersiveScaleFactor[VRSettings::values.vr_immersive_mode]
                                                : immersiveScaleFactor[2];
        const bool isImmersiveModeEnabled = [this](const bool showUIRibbon,
                                                   float&     immersiveModeFactor) -> bool {
            if (VRSettings::values.vr_immersive_mode != 0 &&
                (!showUIRibbon || VRSettings::values.vr_immersive_mode == 1)) {
                UpdateImmersiveModeIfNeeded(immersiveModeFactor);
                return true;
            } else {
                immersiveModeFactor = 1.0f;
                DisableImmersiveMode();
                return false;
            }
        }(showUIRibbon, immersiveModeFactor);

        //////////////////////////////////////////////////
        //  Set the compositor layers for this frame.
        //////////////////////////////////////////////////

        uint32_t                        layerCount = 0;
        std::vector<XrCompositionLayer> layers(gOpenXr->mMaxLayerCount, XrCompositionLayer{});
        {

            // First, add the passthrough layer.
            if (mPassthroughLayer != nullptr) {

                XrCompositionLayerPassthroughFB passthroughLayer = {};
                mPassthroughLayer->Frame(passthroughLayer);
                layers[layerCount++].mPassthrough = passthroughLayer;
            }

            // Game surface (upper and lower panels) are in front of the passthrough layer.
            mGameSurfaceLayer->FrameTopPanel(gOpenXr->mLocalSpace, layers, layerCount,
                                             gOpenXr->headLocation.pose, isImmersiveModeEnabled,
                                             immersiveModeFactor);

            if (showUIRibbon) { mRibbonLayer->Frame(gOpenXr->mLocalSpace, layers, layerCount); }
            const bool showLowerPanel =
                showUIRibbon && appState.mLowerMenuType == LowerMenuType::MAIN_MENU;
            if (showLowerPanel) {
                mGameSurfaceLayer->FrameLowerPanel(gOpenXr->mLocalSpace, layers, layerCount,
                                                   immersiveModeFactor);
            }

            // If active, the keyboard layer is in front of the game surface.
            if (appState.mIsKeyboardActive) {
                mKeyboardLayer->Frame(gOpenXr->mLocalSpace, layers, layerCount);
            }

            // If visible, error messsage appears in front of all other panels.
            if (appState.mShouldShowErrorMessage) {
                mErrorMessageLayer->Frame(gOpenXr->mLocalSpace, layers, layerCount);
            }

            // Cursor visibility will depend on hit-test but will be in front
            // of all other panels. This is because precedence lines up with depth order.
            HandleCursorLayer(jni, appState, showLowerPanel, showUIRibbon, layers, layerCount);
        }

        std::vector<const XrCompositionLayerBaseHeader*> layerHeaders;
        for (uint32_t i = 0; i < layerCount; i++) {
            layerHeaders.push_back((const XrCompositionLayerBaseHeader*)&layers[i]);
        }

        ////////////////////////////////
        // XrEndFrame()
        ////////////////////////////////

        const XrFrameEndInfo endFrameInfo = {XR_TYPE_FRAME_END_INFO,
                                             nullptr,
                                             frameState.predictedDisplayTime,
                                             XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
                                             static_cast<uint32_t>(layerHeaders.size()),
                                             layerHeaders.data()};
        OXR(xrEndFrame(gOpenXr->mSession, &endFrameInfo));
    }

    void HandleInput(JNIEnv* jni, const InputStateFrame& inputState, AppState& newState) const {
        assert(gOpenXr != nullptr);

        // Forward VR input to Android gamepad emulation

        ForwardButtonStateIfNeeded(jni, mActivityObject, mForwardVRInputMethodID, 96 /* BUTTON_A */,
                                   inputState.mAButtonState, "a");
        ForwardButtonStateIfNeeded(jni, mActivityObject, mForwardVRInputMethodID, 97 /* BUTTON_B */,
                                   inputState.mBButtonState, "b");
        ForwardButtonStateIfNeeded(jni, mActivityObject, mForwardVRInputMethodID, 99 /* BUTTON_X */,
                                   inputState.mXButtonState, "x");
        ForwardButtonStateIfNeeded(jni, mActivityObject, mForwardVRInputMethodID,
                                   100 /* BUTTON_Y */, inputState.mYButtonState, "y");
        ForwardButtonStateIfNeeded(
            jni, mActivityObject, mForwardVRInputMethodID, 102 /* BUTTON_L1 */,
            inputState.mSqueezeTriggerState[InputStateFrame::LEFT_CONTROLLER], "l1");
        ForwardButtonStateIfNeeded(
            jni, mActivityObject, mForwardVRInputMethodID, 103 /* BUTTON_R1 */,
            inputState.mSqueezeTriggerState[InputStateFrame::RIGHT_CONTROLLER], "r1");

        {
            // Right is circlepad, left is leftstick/circlepad, dpad is whatever
            // has thumbstick pressed.
            const auto leftStickHand = InputStateFrame::LEFT_CONTROLLER;
            const auto cStickHand    = InputStateFrame::RIGHT_CONTROLLER;

            const auto& leftThumbrestTouchState =
                inputState.mThumbrestTouchState[InputStateFrame::LEFT_CONTROLLER];
            const auto& rightThumbrestTouchState =
                inputState.mThumbrestTouchState[InputStateFrame::RIGHT_CONTROLLER];
            const int dpadHand =
                leftThumbrestTouchState.currentState    ? InputStateFrame::RIGHT_CONTROLLER
                : rightThumbrestTouchState.currentState ? InputStateFrame::LEFT_CONTROLLER
                                                        : InputStateFrame::NUM_CONTROLLERS;

            {
                static constexpr float kThumbStickDirectionThreshold = 0.5f;
                // Doing it this way helps ensure we don't leave the dpad
                // pressed if the thumbstick is released while still
                // pointing in the same direction.
                // I hope this is right, I didn't test it very much.
                static bool wasDpadLeft  = false;
                static bool wasDpadRight = false;
                static bool wasDpadUp    = false;
                static bool wasDpadDown  = false;

                const bool  hasDpad = dpadHand != InputStateFrame::NUM_CONTROLLERS;
                const auto& dpadThumbstickState =
                    inputState.mThumbStickState[hasDpad ? dpadHand : leftStickHand];

                if (hasDpad && dpadThumbstickState.currentState.y > kThumbStickDirectionThreshold) {
                    jni->CallVoidMethod(mActivityObject, mForwardVRInputMethodID, 19 /* DPAD_UP */,
                                        1);
                    wasDpadUp = true;
                } else if (wasDpadUp) {
                    jni->CallVoidMethod(mActivityObject, mForwardVRInputMethodID, 19 /* DPAD_UP */,
                                        0);
                    wasDpadUp = false;
                }
                if (hasDpad &&
                    dpadThumbstickState.currentState.y < -kThumbStickDirectionThreshold) {
                    jni->CallVoidMethod(mActivityObject, mForwardVRInputMethodID,
                                        20 /* DPAD_DOWN */, 1);
                    wasDpadDown = true;
                } else if (wasDpadDown) {
                    jni->CallVoidMethod(mActivityObject, mForwardVRInputMethodID,
                                        20 /* DPAD_DOWN */, 0);
                    wasDpadDown = false;
                }
                if (hasDpad &&
                    dpadThumbstickState.currentState.x < -kThumbStickDirectionThreshold) {
                    jni->CallVoidMethod(mActivityObject, mForwardVRInputMethodID,
                                        21 /* DPAD_LEFT */, 1);
                    wasDpadLeft = true;
                } else if (wasDpadLeft) {
                    jni->CallVoidMethod(mActivityObject, mForwardVRInputMethodID,
                                        21 /* DPAD_LEFT */, 0);
                    wasDpadLeft = false;
                }

                if (hasDpad && dpadThumbstickState.currentState.x > kThumbStickDirectionThreshold) {
                    jni->CallVoidMethod(mActivityObject, mForwardVRInputMethodID,
                                        22 /* DPAD_RIGHT */, 1);
                    wasDpadRight = true;
                } else if (wasDpadRight) {
                    jni->CallVoidMethod(mActivityObject, mForwardVRInputMethodID,
                                        22 /* DPAD_RIGHT */, 0);
                    wasDpadRight = false;
                }
            }

            if (dpadHand != cStickHand) {
                const auto cStickThumbstickState = inputState.mThumbStickState[cStickHand];
                if (cStickThumbstickState.currentState.y != 0 ||
                    cStickThumbstickState.currentState.x != 0 ||
                    cStickThumbstickState.changedSinceLastSync) {
                    jni->CallVoidMethod(mActivityObject, mForwardVRJoystickMethodID,
                                        cStickThumbstickState.currentState.x,
                                        cStickThumbstickState.currentState.y, 0);
                }
            }
            if (dpadHand != leftStickHand) {
                const auto leftStickThumbstickState = inputState.mThumbStickState[leftStickHand];
                if (leftStickThumbstickState.currentState.y != 0 ||
                    leftStickThumbstickState.currentState.x != 0 ||
                    leftStickThumbstickState.changedSinceLastSync) {
                    jni->CallVoidMethod(mActivityObject, mForwardVRJoystickMethodID,
                                        leftStickThumbstickState.currentState.x,
                                        leftStickThumbstickState.currentState.y, 1);
                }
            }
        }

#if !defined(USE_INGAME_MENU)
        if (mInputStateFrame.mLeftMenuButtonState.changedSinceLastSync &&
            mInputStateFrame.mLeftMenuButtonState.currentState == XR_TRUE) {
            newState.mIsLowerMenuToggledOn = !newState.mIsLowerMenuToggledOn;
        }
#endif
    }

    /** Handle the cursor and any hand-tracked/layer-dependent input
     *  interactions.
     **/
    void HandleCursorLayer(JNIEnv* jni, const AppState& appState, const bool showLowerPanel,
                           const bool showUIRibbon, std::vector<XrCompositionLayer>& layers,
                           uint32_t& layerCount) const {

        bool                    shouldRenderCursor = false;
        XrPosef                 cursorPose3d       = XrMath::Posef::Identity();
        XrVector2f              cursorPos2d        = {0, 0};
        float                   scaleFactor        = 0.01f;
        CursorLayer::CursorType cursorType =
            appState.mLowerMenuType == LowerMenuType::POSITIONAL_MENU
                ? CursorLayer::CursorType::CURSOR_TYPE_POSITIONAL_MENU
                : CursorLayer::CursorType::CURSOR_TYPE_NORMAL;

        [[maybe_unused]] const auto nonPreferredController =
            mInputStateFrame.mPreferredHand == InputStateFrame::LEFT_CONTROLLER
                ? InputStateFrame::RIGHT_CONTROLLER
                : InputStateFrame::LEFT_CONTROLLER;
        // assert that we do not choose an inactive controller
        // unless neither controller is available.
        assert(mInputStateFrame.mIsHandActive[mInputStateFrame.mPreferredHand] ||
               !mInputStateFrame.mIsHandActive[nonPreferredController]);
        {
            const bool isPreferredControllerActive =
                mInputStateFrame.mIsHandActive[mInputStateFrame.mPreferredHand];

            static bool sIsLowerPanelBeingPositioned = false;

            sIsLowerPanelBeingPositioned &=
                appState.mLowerMenuType == LowerMenuType::POSITIONAL_MENU &&
                isPreferredControllerActive;
            if (isPreferredControllerActive) {
                const XrPosef pose =
                    mInputStateFrame.mHandPositions[mInputStateFrame.mPreferredHand].pose;
                const auto& triggerState =
                    mInputStateFrame.mIndexTriggerState[mInputStateFrame.mPreferredHand];

                const XrVector3f start = XrMath::Posef::Transform(
                    mInputStateFrame.mHandPositions[mInputStateFrame.mPreferredHand].pose,
                    XrVector3f{0, 0, 0});
                const XrVector3f end = XrMath::Posef::Transform(
                    mInputStateFrame.mHandPositions[mInputStateFrame.mPreferredHand].pose,
                    XrVector3f{0, 0, -3.5f});

                sIsLowerPanelBeingPositioned &= triggerState.currentState > 0;

                // Hit-test panels in order of priority (and known depth)

                // 1. Error message layer
                if (appState.mShouldShowErrorMessage) {
                    shouldRenderCursor = mErrorMessageLayer->GetRayIntersectionWithPanel(
                        start, end, cursorPos2d, cursorPose3d);
                    if (triggerState.changedSinceLastSync) {
                        mErrorMessageLayer->SendClickToUI(cursorPos2d, triggerState.currentState);
                    }
                }

                // 2. Keyboard layer
                if (!shouldRenderCursor && appState.mIsKeyboardActive) {
                    shouldRenderCursor = mKeyboardLayer->GetRayIntersectionWithPanel(
                        start, end, cursorPos2d, cursorPose3d);
                    if (triggerState.changedSinceLastSync) {
                        mKeyboardLayer->SendClickToUI(cursorPos2d, triggerState.currentState);
                    }
                }
                // No dialogs/popups that should impede normal cursor interaction with
                // applicable panels

                // 3. Lower panel
                if (!shouldRenderCursor && showLowerPanel) {
                    shouldRenderCursor = mGameSurfaceLayer->GetRayIntersectionWithPanel(
                        start, end, cursorPos2d, cursorPose3d);
                    if (appState.mLowerMenuType != LowerMenuType::POSITIONAL_MENU) {
                        SendTriggerStateToWindow(jni, mActivityObject, mSendClickToWindowMethodID,
                                                 triggerState, cursorPos2d);
                    }
                }

                // 4. Ribbon layer
                if (!shouldRenderCursor && showUIRibbon) {
                    shouldRenderCursor = mRibbonLayer->GetRayIntersectionWithPanel(
                        start, end, cursorPos2d, cursorPose3d);
                    if (shouldRenderCursor && triggerState.changedSinceLastSync) {
                        mRibbonLayer->SendClickToUI(cursorPos2d, triggerState.currentState);
                    }
                    // If trigger is pressed, thumbstick controls
                    // the depth
                    const XrActionStateVector2f& thumbstickState =
                        mInputStateFrame.mThumbStickState[mInputStateFrame.mPreferredHand];
                    static constexpr float kThumbStickDirectionThreshold = 0.5f;
                    const bool             hasThumbstickMotion =
                        std::abs(thumbstickState.currentState.y) > kThumbStickDirectionThreshold;

                    if (appState.mLowerMenuType == LowerMenuType::POSITIONAL_MENU &&
                        (sIsLowerPanelBeingPositioned ||
                         (shouldRenderCursor && mRibbonLayer->IsMenuBackgroundSelected())) &&
                        (triggerState.currentState || hasThumbstickMotion)) {
                        if (hasThumbstickMotion) {
                            mRibbonLayer->SetPanelFromThumbstick(thumbstickState.currentState.y);
                        } else {
                            mRibbonLayer->SetPanelFromController(
                                {appState.mIsHorizontalAxisLocked ? 0.0f : cursorPose3d.position.x,
                                 cursorPose3d.position.y, cursorPose3d.position.z});
                        }

                        mGameSurfaceLayer->SetLowerPanelWithPose(mRibbonLayer->GetPose());
                        sIsLowerPanelBeingPositioned = true;
                    }
                }

                // 5. Top panel (only if positional menu is active)
                if (!shouldRenderCursor &&
                    appState.mLowerMenuType == LowerMenuType::POSITIONAL_MENU) {
                    shouldRenderCursor = mGameSurfaceLayer->GetRayIntersectionWithPanelTopPanel(
                        start, end, cursorPos2d, cursorPose3d);
                    // If top panel is hit, trigger controls the
                    // position/rotation
                    if (shouldRenderCursor && triggerState.currentState) {
                        // null out X component -- screen should stay
                        // center
                        mGameSurfaceLayer->SetTopPanelFromController(XrVector3f{
                            appState.mIsHorizontalAxisLocked ? 0.0f : cursorPose3d.position.x,
                            cursorPose3d.position.y, cursorPose3d.position.z});
                        // If trigger is pressed, thumbstick controls
                        // the depth
                        const XrActionStateVector2f& thumbstickState =
                            mInputStateFrame.mThumbStickState[mInputStateFrame.mPreferredHand];

                        static constexpr float kThumbStickDirectionThreshold = 0.5f;
                        if (std::abs(thumbstickState.currentState.y) >
                            kThumbStickDirectionThreshold) {
                            mGameSurfaceLayer->SetTopPanelFromThumbstick(
                                thumbstickState.currentState.y);
                        }
                    }
                }

                if (!shouldRenderCursor) {
                    // Handling this here means L2/R2 are liable to
                    // be slightly out of sync with the other
                    // buttons (which are handled before
                    // WaitFrame()). We'll see if that ends up being
                    // a problem for any games.
                    ForwardButtonStateIfNeeded(
                        jni, mActivityObject, mForwardVRInputMethodID, 104 /* BUTTON_L2 */,
                        mInputStateFrame.mIndexTriggerState[InputStateFrame::LEFT_CONTROLLER],
                        "l2");
                    ForwardButtonStateIfNeeded(
                        jni, mActivityObject, mForwardVRInputMethodID, 105 /* BUTTON_R2 */,
                        mInputStateFrame.mIndexTriggerState[InputStateFrame::RIGHT_CONTROLLER],
                        "r2");
                }

                // Add a scale factor so the cursor doesn't scale as
                // quickly as the panel(s) with distance. This may be
                // mildly  unsettling, but it helps to ensure the cursor
                // isn't invisisble at the furthest distance.
                // It's just eyeballed -- no fantastic formula.
                {
                    const float distance =
                        XrMath::Vector3f::Length(pose.position - cursorPose3d.position);
                    scaleFactor = 0.01f + 0.003f * distance;
                }
            }
            if (sIsLowerPanelBeingPositioned) { shouldRenderCursor = true; }
        }

        if (shouldRenderCursor) {
            ALOG_INPUT_VERBOSE("Cursor 2D coords: {} {}", cursorPos2d.x, cursorPos2d.y);
            XrCompositionLayerQuad quadLayer = {};
            mCursorLayer->Frame(gOpenXr->mLocalSpace, quadLayer, cursorPose3d, scaleFactor,
                                cursorType);
            layers[layerCount++].mQuad = quadLayer;
        }
        // FIXME Don't do this right now, because it's confusing to the
        // user. When you exit, the audio is muted until you doff/don,
        // and I don't love that it doesn't close with a single
        // action. So instead, we'll map this to start/select

#if defined(USE_INGAME_MENU)
        if (mInputStateFrame.mLeftMenuButtonState.changedSinceLastSync &&
            mInputStateFrame.mLeftMenuButtonState.currentState == XR_TRUE) {
            jni->CallVoidMethod(mActivityObject, mOpenSettingsMethodID);
        }
#endif
    }

    AppState HandleEvents(JNIEnv* jni) const {
        AppState newState = mLastAppState;
        OXRPollEvents(jni, newState);
        HandleMessageQueueEvents(jni, newState);
        return newState;
    }

    void HandleStateChanges(JNIEnv* jni, AppState& newState) const {
        {
            const bool shouldPauseEmulation =
                !newState.mHasFocus || newState.mShouldShowErrorMessage ||
                newState.mLowerMenuType == LowerMenuType::POSITIONAL_MENU;
            if (shouldPauseEmulation != mLastAppState.mIsEmulationPaused ||
                newState.mLowerMenuType != mLastAppState.mLowerMenuType) {
                ALOGI("State change: Emulation paused: {} -> {} (F={}, E={}, MP={})",
                      mLastAppState.mIsEmulationPaused, shouldPauseEmulation, newState.mHasFocus,
                      newState.mShouldShowErrorMessage,
                      newState.mLowerMenuType == LowerMenuType::POSITIONAL_MENU ? "P"
                      : newState.mLowerMenuType == LowerMenuType::MAIN_MENU     ? "M"
                      : newState.mLowerMenuType == LowerMenuType::STATS_MENU    ? "S"
                                                                                : "U");
                if (shouldPauseEmulation) {
                    PauseEmulation(jni);
                    newState.mIsEmulationPaused = true;
                } else {
                    ResumeEmulation(jni);
                    newState.mIsEmulationPaused = false;
                }
            }
        }

        if (newState.mNumPanelResets > mLastAppState.mNumPanelResets) {
            mGameSurfaceLayer->ResetPanelPositions();
            mRibbonLayer->SetPanelWithPose(mGameSurfaceLayer->GetLowerPanelPose());
        }

        if (newState.mIsHorizontalAxisLocked && !mLastAppState.mIsHorizontalAxisLocked) {
            mGameSurfaceLayer->ResetPanelPositions();
            mRibbonLayer->SetPanelWithPose(mGameSurfaceLayer->GetLowerPanelPose());
        }
    }

    void OXRPollEvents(JNIEnv* jni, AppState& newAppState) const {
        XrEventDataBuffer eventDataBuffer = {};

        // Process all pending messages.
        for (;;) {
            XrEventDataBaseHeader* baseEventHeader = (XrEventDataBaseHeader*)(&eventDataBuffer);
            baseEventHeader->type                  = XR_TYPE_EVENT_DATA_BUFFER;
            baseEventHeader->next                  = NULL;
            XrResult r;
            OXR(r = xrPollEvent(gOpenXr->mInstance, &eventDataBuffer));
            if (r != XR_SUCCESS) { break; }

            switch (baseEventHeader->type) {
                case XR_TYPE_EVENT_DATA_EVENTS_LOST:
                    ALOGV("{}(): Received "
                          "XR_TYPE_EVENT_DATA_EVENTS_LOST "
                          "event",
                          __func__);
                    break;
                case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
                    ALOGV("{}(): Received "
                          "XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING event",
                          __func__);
                    break;
                case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                    const XrEventDataSessionStateChanged* ssce =
                        (XrEventDataSessionStateChanged*)(baseEventHeader);
                    if (ssce != nullptr) {
                        ALOGV("{}(): Received "
                              "XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED",
                              __func__);
                        OXRHandleSessionStateChangedEvent(jni, newAppState, *ssce);
                    } else {
                        ALOGE("{}(): Received "
                              "XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: nullptr",
                              __func__);
                    }
                } break;
                case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
                    ALOGV("{}(): Received "
                          "XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED event",
                          __func__);
                    break;
                case XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT: {
                    [[maybe_unused]] const XrEventDataPerfSettingsEXT* pfs =
                        (XrEventDataPerfSettingsEXT*)(baseEventHeader);
                    ALOGV("{}(): Received "
                          "XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT event: type {} "
                          "subdomain {} : level {} -> level {}",
                          __func__, pfs->type, pfs->subDomain, pfs->fromLevel, pfs->toLevel);
                } break;
                case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
                    ALOGV("{}(): Received "
                          "XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING "
                          "event",
                          __func__);
                    break;
                default:
                    ALOGV("{}(): Unknown event", __func__);
                    break;
            }
        }
    }

    void OXRHandleSessionStateChangedEvent(JNIEnv*                               jni,
                                           AppState&                             newAppState,
                                           const XrEventDataSessionStateChanged& newState) const {
        static XrSessionState lastState = XR_SESSION_STATE_UNKNOWN;
        if (newState.state != lastState) {
            ALOGV("{}(): Received XR_SESSION_STATE_CHANGED state {}->{} "
                  "session={} time={}",
                  __func__, XrSessionStateToString(lastState),
                  XrSessionStateToString(newState.state), (void*)newState.session, newState.time);
        }
        lastState = newState.state;
        switch (newState.state) {
            case XR_SESSION_STATE_FOCUSED:
                ALOGV("{}(): Received XR_SESSION_STATE_FOCUSED event", __func__);
                newAppState.mHasFocus = true;
                break;
            case XR_SESSION_STATE_VISIBLE:
                ALOGV("{}(): Received XR_SESSION_STATE_VISIBLE event", __func__);
                newAppState.mHasFocus = false;
                break;
            case XR_SESSION_STATE_READY:
            case XR_SESSION_STATE_STOPPING:
                OXRHandleSessionStateChanges(newState.state, newAppState);
                break;
            case XR_SESSION_STATE_EXITING:
                newAppState.mIsStopRequested = true;
                break;
            default:
                break;
        }
    }

    void OXRHandleSessionStateChanges(const XrSessionState state, AppState& newAppState) const {
        if (state == XR_SESSION_STATE_READY) {
            assert(mLastAppState.mIsXrSessionActive == false);

            XrSessionBeginInfo sbi           = {};
            sbi.type                         = XR_TYPE_SESSION_BEGIN_INFO;
            sbi.next                         = nullptr;
            sbi.primaryViewConfigurationType = gOpenXr->mViewportConfig.viewConfigurationType;

            XrResult result;
            OXR(result = xrBeginSession(gOpenXr->mSession, &sbi));

            newAppState.mIsXrSessionActive = (result == XR_SUCCESS);

            // Set session state once we have entered VR mode and have a valid
            // session object.
            if (newAppState.mIsXrSessionActive) {
                ALOGI("{}(): Entered XR_SESSION_STATE_READY", __func__);
                PFN_xrPerfSettingsSetPerformanceLevelEXT pfnPerfSettingsSetPerformanceLevelEXT =
                    NULL;
                OXR(xrGetInstanceProcAddr(
                    gOpenXr->mInstance, "xrPerfSettingsSetPerformanceLevelEXT",
                    (PFN_xrVoidFunction*)(&pfnPerfSettingsSetPerformanceLevelEXT)));

                OXR(pfnPerfSettingsSetPerformanceLevelEXT(gOpenXr->mSession,
                                                          XR_PERF_SETTINGS_DOMAIN_CPU_EXT,
                                                          VRSettings::values.cpu_level));
                OXR(pfnPerfSettingsSetPerformanceLevelEXT(
                    gOpenXr->mSession, XR_PERF_SETTINGS_DOMAIN_GPU_EXT, kGpuPerfLevel));
                ALOGI("{}(): Set clock levels to CPU:{}, GPU:{}", __FUNCTION__,
                      VRSettings::values.cpu_level, kGpuPerfLevel);

                PFN_xrSetAndroidApplicationThreadKHR pfnSetAndroidApplicationThreadKHR = NULL;
                OXR(xrGetInstanceProcAddr(
                    gOpenXr->mInstance, "xrSetAndroidApplicationThreadKHR",
                    (PFN_xrVoidFunction*)(&pfnSetAndroidApplicationThreadKHR)));

                if (vr::gPriorityTid > 0) {
                    ALOGD("Setting prio tid from main {}", vr::gPriorityTid);
                    OXR(pfnSetAndroidApplicationThreadKHR(gOpenXr->mSession,
                                                          XR_ANDROID_THREAD_TYPE_RENDERER_MAIN_KHR,
                                                          vr::gPriorityTid));
                } else {
                    ALOGD("Not setting prio tid from main");
                }
                OXR(pfnSetAndroidApplicationThreadKHR(
                    gOpenXr->mSession, XR_ANDROID_THREAD_TYPE_APPLICATION_MAIN_KHR, gettid()));
                if (mGameSurfaceLayer) {
                    ALOGD("SetSurface");
                    mGameSurfaceLayer->SetSurface(mActivityObject);
                }
            }
        } else if (state == XR_SESSION_STATE_STOPPING) {
            assert(mLastAppState.mIsXrSessionActive);
            ALOGI("{}(): Entered XR_SESSION_STATE_STOPPING", __func__);
            OXR(xrEndSession(gOpenXr->mSession));
            newAppState.mIsXrSessionActive = false;
        }
    }

    void HandleMessageQueueEvents(JNIEnv* jni, AppState& newAppState) const {
        // Arbitrary limit to prevent the render thread from blocking too long
        // on a single frame. This may happen if the app is paused in an edge
        // case. We should try to avoid these cases as it will result in a
        // glitchy UX.
        constexpr size_t kMaxNumMessagesPerFrame = 20;

        size_t  numMessagesHandled = 0;
        Message message;
        while (numMessagesHandled < kMaxNumMessagesPerFrame) {
            if (!gMessageQueue.Poll(message)) { break; }
            numMessagesHandled++;

            switch (message.mType) {
                case Message::Type::SHOW_KEYBOARD: {
                    const bool shouldShowKeyboard = message.mPayload == 1;
                    if (shouldShowKeyboard != mLastAppState.mIsKeyboardActive) {
                        ALOGD("Keyboard status changed: {} -> {}", mLastAppState.mIsKeyboardActive,
                              shouldShowKeyboard);
                    }

                    ALOGD("Received SHOW_KEYBOARD message: {}, state change {} -> {}",
                          shouldShowKeyboard, mLastAppState.mIsKeyboardActive, shouldShowKeyboard);
                    newAppState.mIsKeyboardActive = shouldShowKeyboard;

                    break;
                }
                case Message::Type::SHOW_ERROR_MESSAGE: {
                    const bool shouldShowErrorMessage = message.mPayload == 1;
                    ALOGD("Received SHOW_ERROR_MESSAGE message: {}, state change {} -> {}",
                          shouldShowErrorMessage, mLastAppState.mShouldShowErrorMessage,
                          shouldShowErrorMessage);
                    newAppState.mShouldShowErrorMessage = shouldShowErrorMessage;
                    if (newAppState.mShouldShowErrorMessage && !newAppState.mIsEmulationPaused) {
                        ALOGD("Pausing emulation due to error message");
                    }
                    if (!newAppState.mShouldShowErrorMessage && newAppState.mIsEmulationPaused &&
                        newAppState.mHasFocus) {
                        ALOGD("Resuming emulation after error message");
                    }
                    break;
                }
                case Message::Type::EXIT_NEEDED: {
                    ALOGD("Received EXIT_NEEDED message");
                    newAppState.mIsStopRequested = true;
                    break;
                }
                case Message::Type::CHANGE_LOWER_MENU: {
                    newAppState.mLowerMenuType = static_cast<LowerMenuType>(message.mPayload);
                    ALOGD("Received CHANGE_LOWER_MENU message: {}, state change {} -> {}",
                          message.mPayload, mLastAppState.mLowerMenuType,
                          newAppState.mLowerMenuType);
                    break;
                }
                case Message::Type::CHANGE_LOCK_HORIZONTAL_AXIS: {
                    ALOGD("Received CHANGE_LOCK_HORIZONTAL_AXIS message: {}, state change {} -> {}",
                          message.mPayload, mLastAppState.mIsHorizontalAxisLocked,
                          message.mPayload == 1);
                    newAppState.mIsHorizontalAxisLocked = message.mPayload == 1;
                    break;
                }
                case Message::Type::RESET_PANEL_POSITIONS: {
                    ALOGD("Received RESET_PANEL_POSITIONS message");
                    newAppState.mNumPanelResets++;
                    break;
                }
                default:
                    ALOGE("Unknown message type: %d", message.mType);
                    break;
            }
        }
    }

    void PauseEmulation(JNIEnv* jni) const {
        if (!vr::gCitraReady) return;
        assert(jni != nullptr);
        jni->CallVoidMethod(mActivityObject, mPauseGameMethodID);
    }

    void ResumeEmulation(JNIEnv* jni) const {
        if (!vr::gCitraReady) return;
        assert(jni != nullptr);
        jni->CallVoidMethod(mActivityObject, mResumeGameMethodID);
    }

    void UpdateImmersiveModeIfNeeded(const float immersiveModeFactor) const {
        // This block is for testing which uinform offset is needed
        // for a given game to implement new super-immersive profiles if needed
        static bool increase = false;
        static int  uoffset  = -1;
        {
            if (VRSettings::values.vr_immersive_mode > 90) {
                if (mInputStateFrame.mThumbrestTouchState[InputStateFrame::RIGHT_CONTROLLER]
                        .currentState) {
                    if (increase) {
                        ++uoffset;
                        increase = false;
                    }

                    // There are 96 Vec4f; since we are applying 4 of them at a time we need to
                    // loop
                    //  after 92
                    if (uoffset > 92) { uoffset = 0; }
                } else {
                    increase = true;
                }
            }
        }

        if (!Core::System::GetInstance().IsPoweredOn() ||
            !Core::System::GetInstance().GPU().Renderer().Rasterizer()) {
            return;
        }

        XrVector4f transform[4] = {};
        XrMath::Quatf::ToRotationMatrix(gOpenXr->headLocation.pose.orientation, (float*)transform);

        // Calculate the inverse
        XrVector4f inv_transform[4];
        XrMath::Matrixf::ToInverse(transform, inv_transform);

        XrQuaternionf invertedOrientation =
            XrMath::Quatf::Inverted(gOpenXr->headLocation.pose.orientation);
        XrVector3f position =
            XrMath::Quatf::Rotate(invertedOrientation, gOpenXr->headLocation.pose.position);

        const float gamePosScaler =
            powf(10.f, VRSettings::values.vr_immersive_positional_game_scaler) *
            VRSettings::values.vr_factor_3d;

        inv_transform[3].x = -position.x * gamePosScaler;
        inv_transform[3].y = -position.y * gamePosScaler;
        inv_transform[3].z = -position.z * gamePosScaler;

        UpdateImmersiveMode(immersiveModeFactor, uoffset, gamePosScaler, inv_transform);
    }

    void UpdateImmersiveMode(const float immersiveModeFactor, const float uoffset,
                             const float gamePosScaler, const XrVector4f* inv_transform) const {
        Core::System::GetInstance().GPU().Renderer().Rasterizer()->SetVRData(
            VRSettings::values.vr_immersive_mode, immersiveModeFactor, uoffset, -gamePosScaler,
            (float*)inv_transform);
    }

    void DisableImmersiveMode() const {
        if (Core::System::GetInstance().IsPoweredOn() &&
            Core::System::GetInstance().GPU().Renderer().Rasterizer()) {
            Core::System::GetInstance().GPU().Renderer().Rasterizer()->SetVRData(
                1, 1.0f, -1, 0.f, (float*)XrMath::Matrixf::Identity);
        }
    }

    uint64_t    mFrameIndex = 0;
    std::thread mThread;
    jobject     mActivityObject;

    class AppState {
    public:
        LowerMenuType mLowerMenuType          = LowerMenuType::MAIN_MENU;
        int32_t       mNumPanelResets         = 0;
        bool          mIsHorizontalAxisLocked = true;
        bool          mIsLowerMenuToggledOn   = true;

        bool mIsKeyboardActive       = false;
        bool mShouldShowErrorMessage = false;
        bool mIsEmulationPaused      = false;

        bool mIsStopRequested   = false;
        bool mIsXrSessionActive = false;
        bool mHasFocus          = false;
    };

    // App state from previous frame.
    AppState mLastAppState;

    std::unique_ptr<CursorLayer>      mCursorLayer;
    std::unique_ptr<UILayer>          mErrorMessageLayer;
    std::unique_ptr<GameSurfaceLayer> mGameSurfaceLayer;
    std::unique_ptr<PassthroughLayer> mPassthroughLayer;
    std::unique_ptr<UILayer>          mKeyboardLayer;
    std::unique_ptr<RibbonLayer>      mRibbonLayer;

    std::unique_ptr<InputStateStatic> mInputStateStatic;
    InputStateFrame                   mInputStateFrame;

    jmethodID mForwardVRInputMethodID    = nullptr;
    jmethodID mForwardVRJoystickMethodID = nullptr;
    jmethodID mSendClickToWindowMethodID = nullptr;
    jmethodID mResumeGameMethodID        = nullptr;
    jmethodID mPauseGameMethodID         = nullptr;
    jmethodID mOpenSettingsMethodID      = nullptr;
};

//-----------------------------------------------------------------------------
// VRApp

class VRAppThread {
public:
    VRAppThread(JavaVM* jvm, JNIEnv* jni, jobject activityObject)
        : mVm(jvm)
        , mActivityObjectGlobalRef(jni->NewGlobalRef(activityObject)) {
        assert(jvm != nullptr);
        assert(activityObject != nullptr);
        mThread = std::thread([this]() { ThreadFn(); });
    }

    ~VRAppThread() {
        gMessageQueue.Post(Message(Message::Type::EXIT_NEEDED, 0));
        // Note: this is in most cases already going to be true by the time the
        // destructor is called, because it is set to true in onStop()
        ALOGI("Waiting for VRAppThread to join");
        if (mThread.joinable()) { mThread.join(); }
        ALOGI("VRAppThread joined");
    }

private:
    void ThreadFn() {
        assert(mVm != nullptr);
        ALOGI("VRAppThread: starting");
        JNIEnv* jni = nullptr;
        if (mVm->AttachCurrentThread(&jni, nullptr) != JNI_OK) {
            FAIL("%s(): Could not attach to mVm", __FUNCTION__);
        }
        // Gotta set this after the JNIEnv is attached, or else it'll be
        // overwritten
        prctl(PR_SET_NAME, (long)"CVR::Main", 0, 0, 0);

        ThreadFnJNI(jni);

        mVm->DetachCurrentThread();
        ALOGI("VRAppThread: exited");
    }

    // All operations assume that the JNIEnv is attached
    void ThreadFnJNI(JNIEnv* jni) {
        assert(jni != nullptr);
        assert(mActivityObjectGlobalRef != nullptr);
        if (gOpenXr == nullptr) {
            gOpenXr           = std::make_unique<OpenXr>();
            const int32_t ret = gOpenXr->Init(mVm, mActivityObjectGlobalRef);
            if (ret < 0) { FAIL("OpenXR::Init() failed: error code %d", ret); }
        }
        vr::gSession = gOpenXr->mSession;

        { std::make_unique<VRApp>(mActivityObjectGlobalRef)->MainLoop(jni); }

        ALOGI("::MainLoop() exited");

        gOpenXr->Shutdown();

        jni->DeleteGlobalRef(mActivityObjectGlobalRef);
        mActivityObjectGlobalRef = nullptr;
    }

    JavaVM*     mVm;
    jobject     mActivityObjectGlobalRef;
    std::thread mThread;
};

struct VRAppHandle {
    VRAppHandle(VRAppThread* _p)
        : p(_p) {}
    VRAppHandle(jlong _l)
        : l(_l) {}

    union {
        VRAppThread* p = nullptr;
        jlong        l;
    };
};

//-----------------------------------------------------------------------------
// JNI functions

extern "C" JNIEXPORT jlong JNICALL
Java_org_citra_citra_1emu_vr_VrActivity_nativeOnCreate(JNIEnv* env, jobject thiz) {
    // Log the creat start time, which will be used to calculate the total
    // time to first frame.
    gOnCreateStartTime = std::chrono::steady_clock::now();

    JavaVM* jvm;
    env->GetJavaVM(&jvm);
    auto ret = VRAppHandle(new VRAppThread(jvm, env, thiz)).l;
    ALOGI("nativeOnCreate {}", ret);
    return ret;
}
extern "C" JNIEXPORT void JNICALL
Java_org_citra_citra_1emu_vr_VrActivity_nativeOnDestroy(JNIEnv* env, jobject thiz, jlong handle) {
    ALOGI("nativeOnDestroy {}", static_cast<long>(handle));
    if (handle != 0) { delete VRAppHandle(handle).p; }
    VR::JNI::CleanupJNI(env);
}
extern "C" JNIEXPORT jint JNICALL
Java_org_citra_citra_1emu_vr_utils_VRUtils_getHMDType(JNIEnv* env, jclass clazz) {
    return static_cast<jint>(VRSettings::HmdTypeFromStr(VRSettings::GetHMDTypeStr()));
}
extern "C" JNIEXPORT jint JNICALL
Java_org_citra_citra_1emu_vr_utils_VRUtils_getDefaultResolutionFactor(JNIEnv* env, jclass clazz) {
    const VRSettings::HMDType hmdType = VRSettings::HmdTypeFromStr(VRSettings::GetHMDTypeStr());
    return GetDefaultGameResolutionFactorForHmd(hmdType);
}
extern "C" JNIEXPORT void JNICALL Java_org_citra_citra_1emu_vr_utils_VrMessageQueue_nativePost(
    JNIEnv* env, jobject thiz, jint message_type, jlong payload) {
    ALOGI("{}(): message_type: {}, payload: {}", __FUNCTION__, message_type, payload);
    gMessageQueue.Post(Message(static_cast<Message::Type>(message_type), payload));
}

extern "C" JNIEXPORT jfloatArray JNICALL
Java_org_citra_citra_1emu_vr_ui_VrRibbonLayer_nativeGetStatsOXR(JNIEnv* env, jobject thiz) {

    if (vr::gSession == XR_NULL_HANDLE || OpenXr::GetInstance() == XR_NULL_HANDLE) {
        return nullptr;
    }

    static PFN_xrQueryPerformanceMetricsCounterMETA xrQueryPerformanceMetricsCounterMETA = nullptr;
    if (xrQueryPerformanceMetricsCounterMETA == nullptr) {
        const XrResult result =
            xrGetInstanceProcAddr(OpenXr::GetInstance(), "xrQueryPerformanceMetricsCounterMETA",
                                  (PFN_xrVoidFunction*)&xrQueryPerformanceMetricsCounterMETA);
        if (result != XR_SUCCESS) {
            ALOGE("Failed to get xrQueryPerformanceMetrics function pointer: %d", result);
            return nullptr;
        }

        if (xrQueryPerformanceMetricsCounterMETA != nullptr) {
            // do initial query to check available
            PFN_xrSetPerformanceMetricsStateMETA xrSetPerformanceMetricsStateMETA = nullptr;
            xrGetInstanceProcAddr(OpenXr::GetInstance(), "xrSetPerformanceMetricsStateMETA",
                                  (PFN_xrVoidFunction*)&xrSetPerformanceMetricsStateMETA);
            if (xrSetPerformanceMetricsStateMETA != nullptr) {
                XrPerformanceMetricsStateMETA perfMetricsState{
                    XR_TYPE_PERFORMANCE_METRICS_STATE_META};
                perfMetricsState.enabled = XR_TRUE;
                if (xrSetPerformanceMetricsStateMETA(vr::gSession, &perfMetricsState) !=
                    XR_SUCCESS) {
                    ALOGE("xrSetPerformanceMetricsStateMETA failed");
                    return nullptr;
                }
            }
        }
    }
    if (xrQueryPerformanceMetricsCounterMETA == nullptr) {
        ALOGE("xrQueryPerformanceMetricsCounterMETA is not available");
        return nullptr;
    }

    XrPath cpuUtilizationPath, gpuUtilizationPath, appCpuFrameTimePath, appGpuFrameTimePath,
        appVrLatencyPath, compTearPath, compCpuFrameTimePath, compGpuFrameTimePath;

    XrPerformanceMetricsCounterMETA cpuUtilization   = {XR_TYPE_PERFORMANCE_METRICS_COUNTER_META};
    XrPerformanceMetricsCounterMETA gpuUtilization   = {XR_TYPE_PERFORMANCE_METRICS_COUNTER_META};
    XrPerformanceMetricsCounterMETA appCpuFrameTime  = {XR_TYPE_PERFORMANCE_METRICS_COUNTER_META};
    XrPerformanceMetricsCounterMETA appGpuFrameTime  = {XR_TYPE_PERFORMANCE_METRICS_COUNTER_META};
    XrPerformanceMetricsCounterMETA appVrLatency     = {XR_TYPE_PERFORMANCE_METRICS_COUNTER_META};
    XrPerformanceMetricsCounterMETA compTears        = {XR_TYPE_PERFORMANCE_METRICS_COUNTER_META};
    XrPerformanceMetricsCounterMETA compCpuFrameTime = {XR_TYPE_PERFORMANCE_METRICS_COUNTER_META};
    XrPerformanceMetricsCounterMETA compGpuFrameTime = {XR_TYPE_PERFORMANCE_METRICS_COUNTER_META};

    xrStringToPath(OpenXr::GetInstance(), "/perfmetrics_meta/device/cpu_utilization_average",
                   &cpuUtilizationPath);
    xrStringToPath(OpenXr::GetInstance(), "/perfmetrics_meta/device/gpu_utilization",
                   &gpuUtilizationPath);
    xrStringToPath(OpenXr::GetInstance(), "/perfmetrics_meta/app/cpu_frametime",
                   &appCpuFrameTimePath);
    xrStringToPath(OpenXr::GetInstance(), "/perfmetrics_meta/app/gpu_frametime",
                   &appGpuFrameTimePath);
    xrStringToPath(OpenXr::GetInstance(), "/perfmetrics_meta/app/motion_to_photon_latency",
                   &appVrLatencyPath);
    xrStringToPath(OpenXr::GetInstance(), "/perfmetrics_meta/compositor/dropped_frame_count",
                   &compTearPath);
    xrStringToPath(OpenXr::GetInstance(), "/perfmetrics_meta/compositor/cpu_frametime",
                   &compCpuFrameTimePath);
    xrStringToPath(OpenXr::GetInstance(), "/perfmetrics_meta/compositor/gpu_frametime",
                   &compGpuFrameTimePath);

    xrQueryPerformanceMetricsCounterMETA(vr::gSession, cpuUtilizationPath, &cpuUtilization);
    xrQueryPerformanceMetricsCounterMETA(vr::gSession, gpuUtilizationPath, &gpuUtilization);
    xrQueryPerformanceMetricsCounterMETA(vr::gSession, appCpuFrameTimePath, &appCpuFrameTime);
    xrQueryPerformanceMetricsCounterMETA(vr::gSession, appGpuFrameTimePath, &appGpuFrameTime);
    xrQueryPerformanceMetricsCounterMETA(vr::gSession, appVrLatencyPath, &appVrLatency);
    xrQueryPerformanceMetricsCounterMETA(vr::gSession, compTearPath, &compTears);
    xrQueryPerformanceMetricsCounterMETA(vr::gSession, compCpuFrameTimePath, &compCpuFrameTime);
    xrQueryPerformanceMetricsCounterMETA(vr::gSession, compGpuFrameTimePath, &compGpuFrameTime);

    const jfloat metricsArray[8] = {
        cpuUtilization.floatValue,                // Device CPU Utilization %
        gpuUtilization.floatValue,                // Device GPU Utilization %
        appCpuFrameTime.floatValue,               // App CPU Frametime (ms)
        appGpuFrameTime.floatValue,               // App GPU Frametime (ms)
        appVrLatency.floatValue,                  // App VR Latency (ms)
        compCpuFrameTime.floatValue,              // Compositor CPU Frametime (ms)
        compGpuFrameTime.floatValue,              // Compositor GPU Frametime (ms)
        static_cast<jfloat>(compTears.uintValue)  // Compositor tear count
    };

    const jfloatArray result = env->NewFloatArray(8);
    env->SetFloatArrayRegion(result, 0, 8, metricsArray);

    return result;
}
