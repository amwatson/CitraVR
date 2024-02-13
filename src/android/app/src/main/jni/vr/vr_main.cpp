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
#include "vr_settings.h"

#include "utils/Common.h"
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

#include "video_core/renderer_base.h"
#include "video_core/video_core.h"

#if defined(DEBUG_INPUT_VERBOSE)
#define ALOG_INPUT_VERBOSE(...) ALOGI(__VA_ARGS__)
#else
#define ALOG_INPUT_VERBOSE(...)
#endif

#define DECL_PFN(pfn) PFN_##pfn pfn = nullptr
#define INIT_PFN(pfn)                                                                              \
    OXR(xrGetInstanceProcAddr(OpenXr::GetInstance(), #pfn, (PFN_xrVoidFunction*)(&pfn)))

// Used by Citra core to set a higher priority to the non-VR render thread.
namespace vr {
XrSession gSession = XR_NULL_HANDLE;
int gPriorityTid = -1;
XrSession& GetSession() {
    return gSession;
}
void PrioritizeTid(const int tid) {
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
} // namespace vr

namespace {
constexpr XrPerfSettingsLevelEXT kGpuPerfLevel = XR_PERF_SETTINGS_LEVEL_BOOST_EXT;
std::chrono::time_point<std::chrono::steady_clock> gOnCreateStartTime;
std::atomic<bool> gShouldShowErrorMessage = {false};
std::unique_ptr<OpenXr> gOpenXr;

const std::vector<float> immersiveScaleFactor = {1.0f, 5.0f, 3.0f, 1.8f};

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
        ALOGW("Warning: Unsupported HMD type, using default scale factor of {}",
              kDefaultResolutionFactor);
    case VRSettings::HMDType::QUEST2:
    case VRSettings::HMDType::QUESTPRO:
        return kDefaultResolutionFactor;
    }
}

} // anonymous namespace

class VRApp {
public:
    VRApp(JavaVM* jvm, jobject activityObjectGlobalRef)
        : mVm(jvm), mActivityObject(activityObjectGlobalRef), mIsStopRequested(false) {
        assert(mVm != nullptr);
        mThread = std::thread([this]() { MainLoop(); });
    }

    ~VRApp() {
        assert(mVm != nullptr);
        mIsStopRequested = false;
        mThread.join();
        JNIEnv* jni;
        if (mVm->AttachCurrentThread(&jni, nullptr) != JNI_OK) {
            // on most of the android systems, calling exit() isn't like the end
            // of the world. The reapers get to it within a few seconds
            ALOGD("{}() ERROR: could not attach to JVM", __FUNCTION__);
            exit(0);
        }
        jni->DeleteGlobalRef(mActivityObject);
    }

    void MainLoop() {
        JNIEnv* jni;
        if (mVm->AttachCurrentThread(&jni, nullptr) != JNI_OK) {
            FAIL("%s(): Could not attach to JVM", __FUNCTION__);
        }
        mEnv = jni;

        ALOGI("VR Extra Performance Mode: {}",
              VRSettings::values.extra_performance_mode_enabled ? "enabled" : "disabled");
        // Gotta set this after the JNIEnv is attached, or else it'll be
        // overwritten
        prctl(PR_SET_NAME, (long)"CS::Main", 0, 0, 0);
        if (gOpenXr == nullptr) {
            gOpenXr = std::make_unique<OpenXr>();
            const int32_t ret = gOpenXr->Init(mVm, mActivityObject);
            if (ret < 0) {
                FAIL("OpenXR::Init() failed: error code %d", ret);
            }
        }
        vr::gSession = gOpenXr->session_;
        mInputStateStatic =
            std::make_unique<InputStateStatic>(OpenXr::GetInstance(), gOpenXr->session_);

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
            mPassthroughLayer = std::make_unique<PassthroughLayer>(gOpenXr->session_);
        }

        // Create the game surface layer.
        {
            const uint32_t defaultResolutionFactor =
                GetDefaultGameResolutionFactorForHmd(VRSettings::values.hmd_type);
            const uint32_t resolutionFactorFromPreferences = VRSettings::values.resolution_factor;
            // add a couple factors to resolution with immersive mode so users
            // aren't resetting their default settings to get higher res. min
            // resolution factor for immersive is 3x.
            const uint32_t immersiveModeOffset = (VRSettings::values.vr_immersive_mode > 0) ? 2 : 0;
            const uint32_t resolutionFactor = (resolutionFactorFromPreferences > 0
                                                  ? resolutionFactorFromPreferences
                                                  : defaultResolutionFactor) + immersiveModeOffset;

            if (resolutionFactor != defaultResolutionFactor) {
                ALOGI("Using resolution factor of {}x instead of HMD default {}x", resolutionFactor,
                      defaultResolutionFactor);
            }
            mGameSurfaceLayer = std::make_unique<GameSurfaceLayer>(
                XrVector3f{0, 0, -2.0f}, jni, mActivityObject, gOpenXr->session_, resolutionFactor);
        }

        // Create the cursor layer.
        mCursorLayer = std::make_unique<CursorLayer>(gOpenXr->session_);

#if defined(UI_LAYER)
        mErrorMessageLayer = std::make_unique<UILayer>("org/citra/citra_emu/vr/ErrorMessageLayer",
                                                       XrVector3f{0, 0, -0.7}, jni, mActivityObject,
                                                       gOpenXr->session_);
#endif

        //////////////////////////////////////////////////
        // Intialize JNI methods
        //////////////////////////////////////////////////

        mForwardVRInputMethodID =
            jni->GetMethodID(jni->GetObjectClass(mActivityObject), "forwardVRInput", "(IZ)V");
        if (mForwardVRInputMethodID == nullptr) {
            FAIL("could not get forwardVRInputMethodID");
        }
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
        if (mResumeGameMethodID == nullptr) {
            FAIL("could not get resumeGameMethodID");
        }
        mPauseGameMethodID =
            jni->GetMethodID(jni->GetObjectClass(mActivityObject), "pauseGame", "()V");
        if (mPauseGameMethodID == nullptr) {
            FAIL("could not get pauseGameMethodID");
        }
        mOpenSettingsMethodID =
            jni->GetMethodID(jni->GetObjectClass(mActivityObject), "openSettingsMenu", "()V");
        if (mOpenSettingsMethodID == nullptr) {
            FAIL("could not get openSettingsMenuMethodID");
        }

        //////////////////////////////////////////////////
        // Frame loop
        //////////////////////////////////////////////////

        while (!mIsStopRequested) {
            Frame(jni);
        }

        mVm->DetachCurrentThread();
    }

private:
    void Frame(JNIEnv* jni) {
        PollEvents();
        if (mIsStopRequested) {
            return;
        }
        if (!mIsXrSessionActive) {
            // TODO should block here
            mFrameIndex = 0;
            return;
        }
        // Frame index starts at 1. I don't know why, we've always done this.
        // Doesn't actually matter, except to make the indices
        // consistent in traces
        mFrameIndex++;

        // Log time to first frame
        if (mFrameIndex == 1) {
            std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
            ALOGI("Time to first frame: {} ms",
                  std::chrono::duration_cast<std::chrono::milliseconds>(now - gOnCreateStartTime)
                      .count());
        }

        mInputStateFrame.SyncButtonsAndThumbSticks(gOpenXr->session_, mInputStateStatic);

        //////////////////////////////////////////////////
        // Forward VR input to Android gamepad emulation
        //////////////////////////////////////////////////

        ForwardButtonStateIfNeeded(jni, mActivityObject, mForwardVRInputMethodID, 96 /* BUTTON_A */,
                                   mInputStateFrame.mAButtonState, "a");
        ForwardButtonStateIfNeeded(jni, mActivityObject, mForwardVRInputMethodID, 97 /* BUTTON_B */,
                                   mInputStateFrame.mBButtonState, "b");
        ForwardButtonStateIfNeeded(jni, mActivityObject, mForwardVRInputMethodID, 99 /* BUTTON_X */,
                                   mInputStateFrame.mXButtonState, "x");
        ForwardButtonStateIfNeeded(jni, mActivityObject, mForwardVRInputMethodID,
                                   100 /* BUTTON_Y */, mInputStateFrame.mYButtonState, "y");
        ForwardButtonStateIfNeeded(
            jni, mActivityObject, mForwardVRInputMethodID, 102 /* BUTTON_L1 */,
            mInputStateFrame.mSqueezeTriggerState[InputStateFrame::LEFT_CONTROLLER], "l1");
        ForwardButtonStateIfNeeded(
            jni, mActivityObject, mForwardVRInputMethodID, 103 /* BUTTON_R1 */,
            mInputStateFrame.mSqueezeTriggerState[InputStateFrame::RIGHT_CONTROLLER], "r1");

        {
            // Right is circlepad, left is leftstick/circlepad, dpad is whatever
            // has thumbstick pressed.
            const auto leftStickHand = InputStateFrame::LEFT_CONTROLLER;
            const auto cStickHand = InputStateFrame::RIGHT_CONTROLLER;

            const auto& leftThumbrestTouchState =
                mInputStateFrame.mThumbrestTouchState[InputStateFrame::LEFT_CONTROLLER];
            const auto& rightThumbrestTouchState =
                mInputStateFrame.mThumbrestTouchState[InputStateFrame::RIGHT_CONTROLLER];
            const int dpadHand = leftThumbrestTouchState.currentState
                                     ? InputStateFrame::RIGHT_CONTROLLER
                                 : rightThumbrestTouchState.currentState
                                     ? InputStateFrame::LEFT_CONTROLLER
                                     : InputStateFrame::NUM_CONTROLLERS;

            {
                static constexpr float kThumbStickDirectionThreshold = 0.5f;
                // Doing it this way helps ensure we don't leave the dpad
                // pressed if the thumbstick is released while still
                // pointing in the same direction.
                // I hope this is right, I didn't test it very much.
                static bool wasDpadLeft = false;
                static bool wasDpadRight = false;
                static bool wasDpadUp = false;
                static bool wasDpadDown = false;

                const bool hasDpad = dpadHand != InputStateFrame::NUM_CONTROLLERS;
                const auto& dpadThumbstickState =
                    mInputStateFrame.mThumbStickState[hasDpad ? dpadHand : leftStickHand];

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
                const auto cStickThumbstickState = mInputStateFrame.mThumbStickState[cStickHand];
                if (cStickThumbstickState.currentState.y != 0 ||
                    cStickThumbstickState.currentState.x != 0 ||
                    cStickThumbstickState.changedSinceLastSync) {
                    jni->CallVoidMethod(mActivityObject, mForwardVRJoystickMethodID,
                                        cStickThumbstickState.currentState.x,
                                        cStickThumbstickState.currentState.y, 0);
                }
            }
            if (dpadHand != leftStickHand) {
                const auto leftStickThumbstickState =
                    mInputStateFrame.mThumbStickState[leftStickHand];
                if (leftStickThumbstickState.currentState.y != 0 ||
                    leftStickThumbstickState.currentState.x != 0 ||
                    leftStickThumbstickState.changedSinceLastSync) {
                    jni->CallVoidMethod(mActivityObject, mForwardVRJoystickMethodID,
                                        leftStickThumbstickState.currentState.x,
                                        leftStickThumbstickState.currentState.y, 1);
                }
            }
        }

        ////////////////////////////////
        // XrWaitFrame()
        ////////////////////////////////

        // XrWaitFrame returns the predicted display time.
        XrFrameState frameState = {XR_TYPE_FRAME_STATE, nullptr};

        {
            XrFrameWaitInfo wfi = {XR_TYPE_FRAME_WAIT_INFO, nullptr};
            OXR(xrWaitFrame(gOpenXr->session_, &wfi, &frameState));
        }

        ////////////////////////////////
        // XrBeginFrame()
        ////////////////////////////////

        {
            XrFrameBeginInfo bfd = {XR_TYPE_FRAME_BEGIN_INFO, nullptr};
            OXR(xrBeginFrame(gOpenXr->session_, &bfd));
        }

        ///////////////////////////////////////////////////
        // Get tracking, space, projection info for frame.
        ///////////////////////////////////////////////////

        // Re-initialize the reference spaces on the first frame so
        // it is in-sync with user
        if (mFrameIndex == 1) {

            // Create a reference space with the forward direction from the
            // starting frame.
            {
                const XrReferenceSpaceCreateInfo sci = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
                                                        nullptr, XR_REFERENCE_SPACE_TYPE_LOCAL,
                                                        XrMath::Posef::Identity()};
                OXR(xrCreateReferenceSpace(gOpenXr->session_, &sci,
                                           &gOpenXr->forwardDirectionSpace_));
            }

            {
                const XrReferenceSpaceCreateInfo sci = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
                                                        nullptr, XR_REFERENCE_SPACE_TYPE_VIEW,
                                                        XrMath::Posef::Identity()};
                OXR(xrCreateReferenceSpace(gOpenXr->session_, &sci,
                                           &gOpenXr->viewSpace_));
            }

            // Get the pose of the local space.
            XrSpaceLocation lsl = {XR_TYPE_SPACE_LOCATION};
            OXR(xrLocateSpace(gOpenXr->forwardDirectionSpace_, gOpenXr->localSpace_,
                              frameState.predictedDisplayTime, &lsl));

            // Set the forward direction of the new space.
            const XrPosef forwardDirectionPose = lsl.pose;

            // Create a reference space with the same position and rotation as
            // local.
            const XrReferenceSpaceCreateInfo sci = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO, nullptr,
                                                    XR_REFERENCE_SPACE_TYPE_LOCAL,
                                                    forwardDirectionPose};
            OXR(xrCreateReferenceSpace(gOpenXr->session_, &sci, &gOpenXr->headSpace_));
        }

        gOpenXr->headLocation = {XR_TYPE_SPACE_LOCATION};
        OXR(xrLocateSpace(gOpenXr->viewSpace_, gOpenXr->headSpace_, frameState.predictedDisplayTime, &gOpenXr->headLocation));

        mInputStateFrame.SyncHandPoses(gOpenXr->session_, mInputStateStatic, gOpenXr->localSpace_,
                                       frameState.predictedDisplayTime);

        //XrMath::Vector3f::
        XrVector3f leftVec = {
                gOpenXr->headLocation.pose.position.x - mInputStateFrame.mHandPositions[InputStateFrame::LEFT_CONTROLLER].pose.position.x,
                gOpenXr->headLocation.pose.position.y - mInputStateFrame.mHandPositions[InputStateFrame::LEFT_CONTROLLER].pose.position.y,
                gOpenXr->headLocation.pose.position.z - mInputStateFrame.mHandPositions[InputStateFrame::LEFT_CONTROLLER].pose.position.z,
        };
        const float lengthLeft = XrMath::Vector3f::Length(leftVec);
        XrVector3f rightVec = {
                gOpenXr->headLocation.pose.position.x - mInputStateFrame.mHandPositions[InputStateFrame::RIGHT_CONTROLLER].pose.position.x,
                gOpenXr->headLocation.pose.position.y - mInputStateFrame.mHandPositions[InputStateFrame::RIGHT_CONTROLLER].pose.position.y,
                gOpenXr->headLocation.pose.position.z - mInputStateFrame.mHandPositions[InputStateFrame::RIGHT_CONTROLLER].pose.position.z,
        };
        const float lengthRight = XrMath::Vector3f::Length(rightVec);
        const float length = std::min(lengthLeft, lengthRight);

        // This block is for testing which uinform offset is needed
        // for a given game to implement new super-immersive profiles if needed
        static bool increase = false;
        static int uoffset = -1;
        {
            if (VRSettings::values.vr_immersive_mode > 90)
            {
                if (mInputStateFrame.mThumbrestTouchState[InputStateFrame::RIGHT_CONTROLLER].currentState)
                {
                    if (increase)
                    {
                        ++uoffset;
                        increase = false;
                    }

                    //There are 96 Vec4f; since we are applying 4 of them at a time we need to loop
                    // after 92
                    if (uoffset > 92)
                    {
                        uoffset = 0;
                    }
                }
                else
                {
                    increase = true;
                }
            }
        }

        bool showLowerPanel = true;
        float immersiveModeFactor = (VRSettings::values.vr_immersive_mode <= 2) ? immersiveScaleFactor[VRSettings::values.vr_immersive_mode] : immersiveScaleFactor[3];
        // Push the HMD position through to the Rasterizer to pass on to the VS Uniform
        if (VideoCore::g_renderer && VideoCore::g_renderer->Rasterizer())
        {
            if ((VRSettings::values.vr_immersive_positional_factor == 0) ||
                //If in Normal immersive modes then look down for the lower panel to reveal itself (for some reason the Roll function returns pitch)
                (VRSettings::values.vr_immersive_mode <= 2 && XrMath::Quatf::GetRollInRadians(gOpenXr->headLocation.pose.orientation) < -MATH_FLOAT_PI / 8.0f) ||
                //If in "super immersive" mode then put controller next to head in order to disable the mode temporarily
                (VRSettings::values.vr_immersive_mode >= 3 && length < 0.2))
            {
                XrVector4f identity[4] = {};
                XrMath::Matrixf::Identity(identity);
                immersiveModeFactor = 1.0f;
                VideoCore::g_renderer->Rasterizer()->SetVRData(1, immersiveModeFactor, -1, (float*)identity);
            }
            else
            {
                XrVector4f transform[4] = {};
                XrMath::Quatf::ToRotationMatrix(gOpenXr->headLocation.pose.orientation, (float*)transform);

                //Calculate the inverse
                XrVector4f inv_transform[4];
                XrMath::Matrixf::ToInverse(transform, inv_transform);

                XrQuaternionf invertedOrientation = XrMath::Quatf::Inverted(gOpenXr->headLocation.pose.orientation);
                XrVector3f position = XrMath::Quatf::Rotate(invertedOrientation, gOpenXr->headLocation.pose.position);

                float posScaler = powf(10.f, VRSettings::values.vr_immersive_positional_game_scaler);
                inv_transform[3].x = -position.x * VRSettings::values.vr_immersive_positional_factor * posScaler;
                inv_transform[3].y = -position.y * VRSettings::values.vr_immersive_positional_factor * posScaler;
                inv_transform[3].z = -position.z * VRSettings::values.vr_immersive_positional_factor * posScaler;

                VideoCore::g_renderer->Rasterizer()->SetVRData(VRSettings::values.vr_immersive_mode, immersiveModeFactor, uoffset, (float*)inv_transform);
                showLowerPanel = false;
            }
        }

        //////////////////////////////////////////////////
        //  Set the compositor layers for this frame.
        //////////////////////////////////////////////////

        uint32_t layerCount = 0;
        std::vector<XrCompositionLayer> layers(gOpenXr->maxLayerCount_, XrCompositionLayer{});

        // First, add the passthrough layer
        if (mPassthroughLayer != nullptr) {

            XrCompositionLayerPassthroughFB passthroughLayer = {};
            mPassthroughLayer->Frame(passthroughLayer);
            layers[layerCount++].Passthrough = passthroughLayer;
        }

        mGameSurfaceLayer->Frame(gOpenXr->localSpace_, layers, layerCount, gOpenXr->headLocation.pose,
                                 immersiveModeFactor, showLowerPanel);
#if defined(UI_LAYER)
        if (gShouldShowErrorMessage) {
            XrCompositionLayerQuad quadLayer = {};
            mErrorMessageLayer->Frame(gOpenXr->localSpace_, quadLayer);
            layers[layerCount++].Quad = quadLayer;
        }
#endif

        {
            {
                bool shouldRenderCursor = false;
                XrPosef cursorPose3d = XrMath::Posef::Identity();
                XrVector2f cursorPos2d = {0, 0};
                float scaleFactor = 0.01f;
                CursorLayer::CursorType cursorType = CursorLayer::CursorType::CURSOR_TYPE_NORMAL;

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
                        if (gShouldShowErrorMessage) {
#if defined(UI_LAYER)
                            shouldRenderCursor = mErrorMessageLayer->GetRayIntersectionWithPanel(
                                start, end, cursorPos2d, pos3d);
                            position = pos3d;
                            // ALOGI("Cursor 3D pos: {} {} {}",
                            //                                        cursorPos3d.x,
                            //                                        cursorPos3d.y,
                            //                                        cursorPos3d.z);

                            //  ALOGI("Cursor 2D coords: {} {}", cursorPos2d.x,
                            //    cursorPos2d.y);
                            if (triggerState.changedSinceLastSync) {
                                mErrorMessageLayer->SendClickToWindow(cursorPos2d,
                                                                      triggerState.currentState);
                            }
#endif
                        } else {
                            shouldRenderCursor = mGameSurfaceLayer->GetRayIntersectionWithPanel(
                                start, end, cursorPos2d, cursorPose3d);
                            ALOG_INPUT_VERBOSE("Cursor 2D coords: {} {}", cursorPos2d.x,
                                               cursorPos2d.y);
                            if (triggerState.currentState == 0 &&
                                triggerState.changedSinceLastSync) {
                                jni->CallVoidMethod(mActivityObject, mSendClickToWindowMethodID,
                                                    cursorPos2d.x, cursorPos2d.y, 0);
                            } else if (triggerState.changedSinceLastSync &&
                                       triggerState.currentState == 1) {
                                jni->CallVoidMethod(mActivityObject, mSendClickToWindowMethodID,
                                                    cursorPos2d.x, cursorPos2d.y, 1);
                            } else if (triggerState.currentState == 1 &&
                                       !triggerState.changedSinceLastSync) {

                                jni->CallVoidMethod(mActivityObject, mSendClickToWindowMethodID,
                                                    cursorPos2d.x, cursorPos2d.y, 2);
                            }
                            if (!shouldRenderCursor) {
                                // Handling this here means L2/R2 are liable to
                                // be slightly out of sync with the other
                                // buttons (which are handled before
                                // WaitFrame()). We'll see if that ends up being
                                // a problem for any games.
                                ForwardButtonStateIfNeeded(
                                    jni, mActivityObject, mForwardVRInputMethodID,
                                    104 /* BUTTON_L2 */,
                                    mInputStateFrame
                                        .mIndexTriggerState[InputStateFrame::LEFT_CONTROLLER],
                                    "l2");
                                ForwardButtonStateIfNeeded(
                                    jni, mActivityObject, mForwardVRInputMethodID,
                                    105 /* BUTTON_R2 */,
                                    mInputStateFrame
                                        .mIndexTriggerState[InputStateFrame::RIGHT_CONTROLLER],
                                    "r2");
                            }
                        }

                        // Hit test the top panel
                        if (!shouldRenderCursor) {
                            shouldRenderCursor =
                                mGameSurfaceLayer->GetRayIntersectionWithPanelTopPanel(
                                    start, end, cursorPos2d, cursorPose3d);
                            // If top panel is hit, trigger controls the
                            // position/rotation
                            if (shouldRenderCursor && triggerState.currentState) {
                                // null out X component -- screen should stay
                                // center
                                mGameSurfaceLayer->SetTopPanelFromController(XrVector3f{
                                    0, cursorPose3d.position.y, cursorPose3d.position.z});
                                // If trigger is pressed, thumbstick controls
                                // the depth
                                const XrActionStateVector2f& thumbstickState =
                                    mInputStateFrame
                                        .mThumbStickState[mInputStateFrame.mPreferredHand];

                                static constexpr float kThumbStickDirectionThreshold = 0.5f;
                                if (std::abs(thumbstickState.currentState.y) >
                                    kThumbStickDirectionThreshold) {
                                    mGameSurfaceLayer->SetTopPanelFromThumbstick(
                                        thumbstickState.currentState.y);
                                }
                            }
                            if (shouldRenderCursor) {
                                cursorType = CursorLayer::CursorType::CURSOR_TYPE_TOP_PANEL;
                            }
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
                }

                if (shouldRenderCursor) {
                    XrCompositionLayerQuad quadLayer = {};
                    mCursorLayer->Frame(gOpenXr->localSpace_, quadLayer, cursorPose3d, scaleFactor,
                                        cursorType);
                    layers[layerCount++].mQuad = quadLayer;
                }
                // FIXME Don't do this right now, because it's confusing to the
                // user. When you exit, the audio is muted until you doff/don,
                // and I don't love that it doesn't close with a single
                // action. So instead, we'll map this to start/select

#if defined(USE_INGAME_MENU)
                if (mInputStateFrame.mLeftMenuButtonState.changedSinceLastSync ||
                    mInputStateFrame.mLeftMenuButtonState.currentState == XR_TRUE) {
                    jni->CallVoidMethod(mActivityObject, mOpenSettingsMethodID);
                }
#else
                // What would be ideal is if we placed these buttons in the
                // scene, on a layer (maybe as part of the top layer in a view
                // overlay -- I could
                // add a black border to the top/bottom. Don't want to change
                // too much right now. That would have been smart, though.)
                ForwardButtonStateIfNeeded(jni, mActivityObject, mForwardVRInputMethodID,
                                           108 /* BUTTON_START */,
                                           mInputStateFrame.mLeftMenuButtonState, "start");

#endif
            }
        }
        std::vector<const XrCompositionLayerBaseHeader*> layerHeaders;
        for (uint32_t i = 0; i < layerCount; i++) {
            layerHeaders.push_back((const XrCompositionLayerBaseHeader*)&layers[i]);
        }

        ////////////////////////////////
        // XrEndFrame()
        ////////////////////////////////

        const XrFrameEndInfo endFrameInfo = {
            XR_TYPE_FRAME_END_INFO,           nullptr,    frameState.predictedDisplayTime,
            XR_ENVIRONMENT_BLEND_MODE_OPAQUE, layerCount, layerHeaders.data()};
        OXR(xrEndFrame(gOpenXr->session_, &endFrameInfo));
    }

    void HandleSessionStateChanges(const XrSessionState state) {
        if (state == XR_SESSION_STATE_READY) {
            assert(mIsXrSessionActive == false);

            XrSessionBeginInfo sbi = {};
            sbi.type = XR_TYPE_SESSION_BEGIN_INFO;
            sbi.next = nullptr;
            sbi.primaryViewConfigurationType = gOpenXr->viewportConfig_.viewConfigurationType;

            XrResult result;
            OXR(result = xrBeginSession(gOpenXr->session_, &sbi));

            mIsXrSessionActive = (result == XR_SUCCESS);

            // Set session state once we have entered VR mode and have a valid
            // session object.
            if (mIsXrSessionActive) {
                PFN_xrPerfSettingsSetPerformanceLevelEXT pfnPerfSettingsSetPerformanceLevelEXT =
                    NULL;
                OXR(xrGetInstanceProcAddr(
                    gOpenXr->instance_, "xrPerfSettingsSetPerformanceLevelEXT",
                    (PFN_xrVoidFunction*)(&pfnPerfSettingsSetPerformanceLevelEXT)));

                OXR(pfnPerfSettingsSetPerformanceLevelEXT(gOpenXr->session_,
                                                          XR_PERF_SETTINGS_DOMAIN_CPU_EXT,
                                                          VRSettings::values.cpu_level));
                OXR(pfnPerfSettingsSetPerformanceLevelEXT(
                    gOpenXr->session_, XR_PERF_SETTINGS_DOMAIN_GPU_EXT, kGpuPerfLevel));
                ALOGI("{}(): Set clock levels to CPU:{}, GPU:{}", __FUNCTION__,
                      VRSettings::values.cpu_level, kGpuPerfLevel);

                PFN_xrSetAndroidApplicationThreadKHR pfnSetAndroidApplicationThreadKHR = NULL;
                OXR(xrGetInstanceProcAddr(
                    gOpenXr->instance_, "xrSetAndroidApplicationThreadKHR",
                    (PFN_xrVoidFunction*)(&pfnSetAndroidApplicationThreadKHR)));

                if (vr::gPriorityTid > 0) {
                    ALOGD("Setting prio tid from main {}", vr::gPriorityTid);
                    OXR(pfnSetAndroidApplicationThreadKHR(gOpenXr->session_,
                                                          XR_ANDROID_THREAD_TYPE_RENDERER_MAIN_KHR,
                                                          vr::gPriorityTid));
                } else {
                    ALOGD("Not setting prio tid from main");
                }
                OXR(pfnSetAndroidApplicationThreadKHR(
                    gOpenXr->session_, XR_ANDROID_THREAD_TYPE_APPLICATION_MAIN_KHR, gettid()));
                if (mGameSurfaceLayer) {
                    ALOGD("SetSurface");
                    mGameSurfaceLayer->SetSurface();
                }
            }
        } else if (state == XR_SESSION_STATE_STOPPING) {
            assert(mIsXrSessionActive);
            OXR(xrEndSession(gOpenXr->session_));
            mIsXrSessionActive = false;
        }
    }

    void HandleSessionStateChangedEvent(const XrEventDataSessionStateChanged& newState) {
        static XrSessionState lastState = XR_SESSION_STATE_UNKNOWN;
        if (newState.state != lastState) {
            ALOGV("{}(): Received XR_SESSION_STATE_CHANGED state {}->{} "
                  "session={} time={}",
                  __func__, XrSessionStateToString(lastState),
                  XrSessionStateToString(newState.state), newState.session, newState.time);
        }
        lastState = newState.state;
        switch (newState.state) {
        case XR_SESSION_STATE_FOCUSED:
            ALOGV("{}(): Received XR_SESSION_STATE_FOCUSED event", __func__);
            if (!mHasFocus) {
                mEnv->CallVoidMethod(mActivityObject, mResumeGameMethodID);
            }
            mHasFocus = true;
            break;
        case XR_SESSION_STATE_VISIBLE:
            ALOGV("{}(): Received XR_SESSION_STATE_VISIBLE event", __func__);
            if (mHasFocus) {
                mEnv->CallVoidMethod(mActivityObject, mPauseGameMethodID);
            }
            mHasFocus = false;
            break;
        case XR_SESSION_STATE_READY:
        case XR_SESSION_STATE_STOPPING:
            HandleSessionStateChanges(newState.state);
            break;
        case XR_SESSION_STATE_EXITING:
            mIsStopRequested = true;
            break;
        default:
            break;
        }
    }

    void PollEvents() {
        XrEventDataBuffer eventDataBuffer = {};

        // Process all pending messages.
        for (;;) {
            XrEventDataBaseHeader* baseEventHeader = (XrEventDataBaseHeader*)(&eventDataBuffer);
            baseEventHeader->type = XR_TYPE_EVENT_DATA_BUFFER;
            baseEventHeader->next = NULL;
            XrResult r;
            OXR(r = xrPollEvent(gOpenXr->instance_, &eventDataBuffer));
            if (r != XR_SUCCESS) {
                break;
            }

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
                    HandleSessionStateChangedEvent(*ssce);
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

    uint64_t mFrameIndex = 0;
    std::thread mThread;
    JNIEnv* mEnv;
    JavaVM* mVm;
    jobject mActivityObject;
    std::atomic<bool> mIsStopRequested = {false};
    bool mIsXrSessionActive = false;
    bool mHasFocus = false;
    std::unique_ptr<CursorLayer> mCursorLayer;
#if defined(UI_LAYER)
    std::unique_ptr<UILayer> mErrorMessageLayer;
#endif
    std::unique_ptr<GameSurfaceLayer> mGameSurfaceLayer;
    std::unique_ptr<PassthroughLayer> mPassthroughLayer;

    std::unique_ptr<InputStateStatic> mInputStateStatic;
    InputStateFrame mInputStateFrame;

    jmethodID mForwardVRInputMethodID = nullptr;
    jmethodID mForwardVRJoystickMethodID = nullptr;
    jmethodID mSendClickToWindowMethodID = nullptr;
    jmethodID mResumeGameMethodID = nullptr;
    jmethodID mPauseGameMethodID = nullptr;
    jmethodID mOpenSettingsMethodID = nullptr;
};

struct VRAppHandle {
    VRAppHandle(VRApp* _p) : p(_p) {}
    VRAppHandle(jlong _l) : l(_l) {}

    union {
        VRApp* p = nullptr;
        jlong l;
    };
};

extern "C" JNIEXPORT jlong JNICALL
Java_org_citra_citra_1emu_vr_VrActivity_nativeOnCreate(JNIEnv* env, jobject thiz) {
    // Log the creat start time, which will be used to calculate the total
    // time to first frame.
    gOnCreateStartTime = std::chrono::steady_clock::now();

    JavaVM* jvm;
    env->GetJavaVM(&jvm);
    return VRAppHandle(new VRApp(jvm, env->NewGlobalRef(thiz))).l;
}

extern "C" JNIEXPORT void JNICALL
Java_org_citra_citra_1emu_vr_VrActivity_nativeOnDestroy(JNIEnv* env, jlong handle) {

    ALOGI("nativeOnDestroy");
    exit(0);
    if (handle != 0) {
        delete VRAppHandle(handle).p;
    }
    // Even though OpenXR is created on a different thread, this
    // should be ok because thread exit is a fence, and the delete waits to join.
    if (gOpenXr != nullptr) {
        gOpenXr->Shutdown();
    }
}

extern "C" JNIEXPORT void JNICALL Java_org_citra_citra_1emu_vr_ErrorMessageLayer_showErrorWindow(
    JNIEnv* env, jobject thiz, jboolean should_show_error) {
    gShouldShowErrorMessage = should_show_error;
}

extern "C" JNIEXPORT jint JNICALL Java_org_citra_citra_1emu_vr_VRUtils_getHMDType(JNIEnv* env,
                                                                                  jclass clazz) {
    return static_cast<jint>(VRSettings::HmdTypeFromStr(VRSettings::GetHMDTypeStr()));
}
extern "C" JNIEXPORT jint JNICALL
Java_org_citra_citra_1emu_vr_VRUtils_getDefaultResolutionFactor(JNIEnv* env, jclass clazz) {
    const VRSettings::HMDType hmdType = VRSettings::HmdTypeFromStr(VRSettings::GetHMDTypeStr());
    return GetDefaultGameResolutionFactorForHmd(hmdType);
}
