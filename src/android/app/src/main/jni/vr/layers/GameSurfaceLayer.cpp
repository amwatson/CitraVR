/*******************************************************************************

Filename    :   GameSurfaceLayer.cpp

Content     :   Handles the projection of the "Game Surface" panels into XR.
                Includes the "top" panel (stereo game screen) and the "bottom"
                panel (mono touchpad).

Authors     :   Amanda M. Watson
License     :   Licensed under GPLv3 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#include "GameSurfaceLayer.h"

#include "../vr_settings.h"

#include "../utils/JniUtils.h"
#include "../utils/LogUtils.h"
#include "../utils/SyspropUtils.h"
#include "../utils/XrMath.h"

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <chrono>

#include <stdlib.h>

namespace {

constexpr float kSuperImmersiveRadius = 0.5f;

//-----------------------------------------------------------------------------
// Local sysprops

static constexpr std::chrono::milliseconds kMinTimeBetweenChecks(500);

// Get density on an interval
float GetDensitySysprop(const uint32_t resolutionFactor) {
    const float  kDefaultDensity = GameSurfaceLayer::DEFAULT_QUAD_DENSITY * resolutionFactor;
    static float lastDensity     = kDefaultDensity;
    static std::chrono::time_point<std::chrono::steady_clock> lastTime = {};
    // Only check the sysprop every 500ms
    if ((std::chrono::steady_clock::now() - lastTime) >= kMinTimeBetweenChecks) {
        lastTime    = std::chrono::steady_clock::now();
        lastDensity = SyspropUtils::GetSysPropAsFloat("debug.citra.density", kDefaultDensity);
    }
    return lastDensity;
}

int32_t GetCylinderSysprop() {
    static constexpr int32_t                                  kDefaultCylinder = 0;
    static int32_t                                            lastCylinder     = kDefaultCylinder;
    static std::chrono::time_point<std::chrono::steady_clock> lastTime         = {};
    // Only check the sysprop every 500ms
    if ((std::chrono::steady_clock::now() - lastTime) >= kMinTimeBetweenChecks) {
        lastTime     = std::chrono::steady_clock::now();
        lastCylinder = SyspropUtils::GetSysPropAsInt("debug.citra.cylinder", kDefaultCylinder);
    }
    return lastCylinder;
}

float GetRadiusSysprop() {
    static constexpr float kDefaultRadius = GameSurfaceLayer::DEFAULT_CYLINDER_RADIUS;
    static float           lastRadius     = kDefaultRadius;
    static std::chrono::time_point<std::chrono::steady_clock> lastTime = {};
    // Only check the sysprop every 500ms
    if ((std::chrono::steady_clock::now() - lastTime) >= kMinTimeBetweenChecks) {
        lastTime   = std::chrono::steady_clock::now();
        lastRadius = SyspropUtils::GetSysPropAsFloat("debug.citra.radius", kDefaultRadius);
    }
    return lastRadius;
}

float GetCentralAngleSysprop() {
    static constexpr float kDefaultCentralAngle =
        GameSurfaceLayer::DEFAULT_CYLINDER_CENTRAL_ANGLE_DEGREES;
    static float lastCentralAngle                                      = kDefaultCentralAngle;
    static std::chrono::time_point<std::chrono::steady_clock> lastTime = {};
    // Only check the sysprop every 500ms
    if ((std::chrono::steady_clock::now() - lastTime) >= kMinTimeBetweenChecks) {
        lastTime = std::chrono::steady_clock::now();
        lastCentralAngle =
            SyspropUtils::GetSysPropAsFloat("debug.citra.cylinder_degrees", kDefaultCentralAngle);
    }
    return lastCentralAngle;
}

//-----------------------------------------------------------------------------
// Panel math

/** Define the space of the top panel.
 *
 * @param position The position of the panel in world space.
 * @param surfaceWidth The width of the swapchain surface.
 * @param surfaceHeight The height of the swapchain surface.
 *
 */
Panel CreateTopPanel(const XrVector3f& position, const float surfaceWidth,
                     const float surfaceHeight) {
    // Half the surface width because the panel is rendered in stereo, and each eye gets half the
    // total width.
    const float panelWidth = surfaceWidth / 2.0f;
    // Half the surface height because the surface is divided in half between the upper and lower
    // panels (See GameSurfaceLayer.h for more info about how the surface is divided).
    const float panelHeight = surfaceHeight / 2.0f;

    return Panel(XrPosef{XrMath::Quatf::Identity(), position}, panelWidth, panelHeight, 1.0f);
}

/** Define the space of the lower panel.
 *    - Below the top panel
 *    - Rotated 45 degrees away from viewer
 *    - 1.5m away from viewer
 *    - Scaled down to 75%^2 of the top panel's size
 *      - Note: this is an arbitrary scale constant, chosen
 *        because the scale (supposed to be .75) was incorrectly
 *        squared when I did the ribbon positioning math.
 *
 *    Note: all values are chosen to be aesthetically pleasing and can be modified.
 */
Panel CreateLowerPanelFromTopPanel(const Panel& topPanel, const float resolutionFactor) {
    // Note: the fact that two constants are 0.75 is purely coincidental.
    constexpr float kDefaultLowerPanelScaleFactor = 0.75f * 0.75f;
    constexpr float kLowerPanelYOffsetInMeters    = -0.75f;
    constexpr float kLowerPanelZOffsetInMeters    = -1.5f;
    // Pitch the lower panel away from the viewer 45 degrees
    constexpr float kLowerPanelPitchInRadians = -MATH_FLOAT_PI / 4.0f;
    const float     cropHoriz                 = 90.0f * resolutionFactor;

    XrPosef lowerPanelFromWorld = topPanel.mPanelFromWorld;
    lowerPanelFromWorld.orientation =
        XrMath::Quatf::FromEuler(0.0f, kLowerPanelPitchInRadians, 0.0f);
    lowerPanelFromWorld.position.y += kLowerPanelYOffsetInMeters;
    lowerPanelFromWorld.position.z = kLowerPanelZOffsetInMeters;
    return Panel(lowerPanelFromWorld, topPanel.mWidth, topPanel.mHeight,
                 kDefaultLowerPanelScaleFactor, XrVector2f{cropHoriz / 2.0f, 0.0f},
                 XrVector2f{topPanel.mWidth - cropHoriz / 2.0f, topPanel.mHeight});
}

XrVector3f CalculatePanelPosition(const XrVector3f& viewerPosition,
                                  const XrVector3f& controllerPosition,
                                  float             sphereRadius) {
    // Vector from viewer to controller
    XrVector3f viewerToController = controllerPosition - viewerPosition;
    XrMath::Vector3f::Normalize(viewerToController);

    // Calculate position on the sphere's surface
    return viewerPosition + sphereRadius * viewerToController;
}

XrQuaternionf CalculatePanelRotation(const XrVector3f& windowPosition,
                                     const XrVector3f& viewerPosition,
                                     const XrVector3f& upDirection) {
    // Compute forward direction (normalized)
    XrVector3f forward = viewerPosition - windowPosition;
    XrMath::Vector3f::Normalize(forward);

    // Compute right direction (normalized)
    XrVector3f right = XrMath::Vector3f::Cross(upDirection, forward);
    XrMath::Vector3f::Normalize(right);

    // Recompute up direction (normalized) to ensure orthogonality
    const XrVector3f up = XrMath::Vector3f::Cross(forward, right);

    return XrMath::Quatf::FromThreeVectors(forward, up, right);
}

bool GetRayIntersectionWithPanel(const Panel& panel, const XrVector2f& scaleFactor,
                                 const XrVector3f& start, const XrVector3f& end,
                                 XrVector2f& result2d, XrPosef& result3d,
                                 const float resolutionFactor)

{
    const XrPosef    worldFromPanel = XrMath::Posef::Inverted(panel.mPanelFromWorld);
    const XrVector3f localStart     = XrMath::Posef::Transform(worldFromPanel, start);
    const XrVector3f localEnd       = XrMath::Posef::Transform(worldFromPanel, end);
    // Note: assumes layer lies in the XZ plane
    const float tan = localStart.z / (localStart.z - localEnd.z);

    // Check for backwards controller
    if (tan < 0) {
        ALOGD("Backwards controller");
        return false;
    }
    result3d.position    = start + (end - start) * tan;
    result3d.orientation = panel.mPanelFromWorld.orientation;

    const XrVector2f result2dNDC = {
        (localStart.x + (localEnd.x - localStart.x) * tan) / (scaleFactor.x),
        (localStart.y + (localEnd.y - localStart.y) * tan) / (scaleFactor.y)};

    panel.Transform(result2dNDC, result2d);
    const bool isInBounds =
        result2d.x >= panel.mClickBounds.mMin.x && result2d.y >= panel.mClickBounds.mMin.y &&
        result2d.x < panel.mClickBounds.mMax.x && result2d.y < panel.mClickBounds.mMax.x;
    result2d.y += panel.mHeight;

    if (!isInBounds) { return false; }

    return true;
}

// Uses a density for scaling and sets aspect ratio
XrVector2f GetDensityScaleForSize(const int32_t  texWidth,
                                  const int32_t  texHeight,
                                  const float    scaleFactor,
                                  const uint32_t resolutionFactor) {
    const float density = GetDensitySysprop(resolutionFactor);
    return XrVector2f{2.0f * static_cast<float>(texWidth) / density,
                      (static_cast<float>(texHeight) / density)} *
           scaleFactor;
}

} // anonymous namespace

/*
================================================================================

 Panel

================================================================================
*/

void Panel::Transform(const XrVector2f& point2d, XrVector2f& result) const {

    result.x = (point2d.x * mWidth) + (mWidth / 2.0f);
    // Android has a flipped vertical axis from OpenXR
    result.y = ((1.0f - point2d.y) * mHeight) - (mHeight / 2.0f);
}

/*
================================================================================

 GameSurfaceLayer

================================================================================
*/

GameSurfaceLayer::GameSurfaceLayer(const XrVector3f&& position, JNIEnv* env, jobject activityObject,
                                   const XrSession& session, const uint32_t resolutionFactor)
    : mSession(session)
    , mResolutionFactor(resolutionFactor)
    , mTopPanel(CreateTopPanel(position,
                               (SURFACE_WIDTH_UNSCALED * mResolutionFactor),
                               (SURFACE_HEIGHT_UNSCALED * mResolutionFactor)))
    , mLowerPanel(CreateLowerPanelFromTopPanel(mTopPanel, mResolutionFactor))
    , mImmersiveMode(VRSettings::values.vr_immersive_mode)
    , mEnv(env)

{
    const int32_t initializationStatus = Init(session, activityObject);
    if (initializationStatus < 0) {
        FAIL("Could not initialize GameSurfaceLayer -- error '%d'", initializationStatus);
    }
}

GameSurfaceLayer::~GameSurfaceLayer() { Shutdown(); }

void GameSurfaceLayer::SetSurface(const jobject activityObject) const {
    assert(mVrGameSurfaceClass != nullptr);

    const jmethodID setSurfaceMethodID =
        mEnv->GetStaticMethodID(mVrGameSurfaceClass, "setSurface",
                                "(Lorg/citra/citra_emu/vr/VrActivity;Landroid/view/Surface;)V");
    if (setSurfaceMethodID == nullptr) { FAIL("Couldn't find setSurface()"); }
    mEnv->CallStaticVoidMethod(mVrGameSurfaceClass, setSurfaceMethodID, activityObject, mSurface);
}

void GameSurfaceLayer::Frame(const XrSpace& space, std::vector<XrCompositionLayer>& layers,
                             uint32_t& layerCount, const XrPosef& headPose,
                             const float& immersiveModeFactor, const bool showLowerPanel) {
    // Prevent a seam between the top and bottom view
    constexpr uint32_t verticalBorderTex = 1;
    const bool         useCylinder       = (GetCylinderSysprop() != 0) || (mImmersiveMode > 0);
    if (useCylinder) {
        // Create the Top Display Panel (Curved display)
        for (uint32_t eye = 0; eye < NUM_EYES; eye++) {
            XrPosef topPanelFromWorld = mTopPanel.mPanelFromWorld;
            if (mImmersiveMode > 1 && !showLowerPanel) {
                topPanelFromWorld = GetTopPanelFromHeadPose(eye, headPose);
            }

            XrCompositionLayerCylinderKHR layer = {};

            layer.type       = XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR;
            layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            layer.layerFlags |= XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
            layer.layerFlags |= XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
            // NOTE: may not want unpremultiplied alpha

            layer.space = space;

            // Radius effectively controls the width of the cylinder shape.
            // Central angle controls how much of the cylinder is
            // covered by the texture. Together, they control the
            // scale of the texture.
            const float radius =
                (mImmersiveMode < 2 || showLowerPanel) ? GetRadiusSysprop() : kSuperImmersiveRadius;

            layer.eyeVisibility = eye == 0 ? XR_EYE_VISIBILITY_LEFT : XR_EYE_VISIBILITY_RIGHT;
            memset(&layer.subImage, 0, sizeof(XrSwapchainSubImage));
            layer.subImage.swapchain               = mSwapchain.mHandle;
            layer.subImage.imageRect.offset.x      = eye == 0 ? 0 : mTopPanel.mWidth;
            layer.subImage.imageRect.offset.y      = 0;
            layer.subImage.imageRect.extent.width  = mTopPanel.mWidth;
            layer.subImage.imageRect.extent.height = mTopPanel.mHeight - verticalBorderTex;
            layer.subImage.imageArrayIndex         = 0;
            layer.pose                             = topPanelFromWorld;
            layer.pose.position.z += (mImmersiveMode < 2) ? radius : 0.0f;
            layer.radius = radius;
            layer.centralAngle =
                (!mImmersiveMode ? GetCentralAngleSysprop()
                                 : GameSurfaceLayer::DEFAULT_CYLINDER_CENTRAL_ANGLE_DEGREES *
                                       immersiveModeFactor) *
                MATH_FLOAT_PI / 180.0f;
            layer.aspectRatio              = -static_cast<double>(mTopPanel.AspectRatio());
            layers[layerCount++].mCylinder = layer;
        }
    } else {
        // Create the Top Display Panel (Flat display)
        for (uint32_t eye = 0; eye < 2; eye++) {
            const uint32_t         cropHoriz = 50 * mResolutionFactor;
            XrCompositionLayerQuad layer     = {};

            layer.type       = XR_TYPE_COMPOSITION_LAYER_QUAD;
            layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            layer.layerFlags |= XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
            layer.layerFlags |= XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
            // NOTE: may not want unpremultiplied alpha

            layer.space = space;

            layer.eyeVisibility = eye == 0 ? XR_EYE_VISIBILITY_LEFT : XR_EYE_VISIBILITY_RIGHT;
            memset(&layer.subImage, 0, sizeof(XrSwapchainSubImage));
            layer.subImage.swapchain          = mSwapchain.mHandle;
            layer.subImage.imageRect.offset.x = (eye == 0 ? 0 : mTopPanel.mWidth) + cropHoriz / 2;
            layer.subImage.imageRect.offset.y = 0;
            layer.subImage.imageRect.extent.width  = mTopPanel.mWidth - cropHoriz;
            layer.subImage.imageRect.extent.height = mTopPanel.mHeight - verticalBorderTex;
            layer.subImage.imageArrayIndex         = 0;
            layer.pose                             = mTopPanel.mPanelFromWorld;
            // Scale to get the desired density within the visible area (if we
            // want).
            const auto scale  = GetDensityScaleForSize(mTopPanel.mWidth - cropHoriz,
                                                       -mTopPanel.mHeight, 1.0f, mResolutionFactor);
            layer.size.width  = scale.x;
            layer.size.height = scale.y;

            layers[layerCount++].mQuad = layer;
        }
    }

    // Create the Lower Display Panel (flat touchscreen)
    // When citra is in stereo mode, this panel is also rendered in stereo (i.e.
    // twice), but the image is mono. Therefore, take the right half of the
    // screen and use it for both eyes.
    // FIXME we waste rendering time rendering both displays. That said, We also
    // waste rendering time copying the buffer between runtimes. No time for
    // that now!
    if (showLowerPanel) {
        const uint32_t         cropHoriz = 90 * mResolutionFactor;
        XrCompositionLayerQuad layer     = {};

        layer.type       = XR_TYPE_COMPOSITION_LAYER_QUAD;
        layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        layer.layerFlags |= XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
        layer.layerFlags |= XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
        // NOTE: may not want unpremultiplied alpha

        layer.space = space;

        layer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
        memset(&layer.subImage, 0, sizeof(XrSwapchainSubImage));
        layer.subImage.swapchain = mSwapchain.mHandle;
        layer.subImage.imageRect.offset.x =
            (cropHoriz / 2) / immersiveModeFactor +
            mLowerPanel.mWidth * (0.5f - (0.5f / immersiveModeFactor));
        layer.subImage.imageRect.offset.y =
            mLowerPanel.mHeight + verticalBorderTex +
            mLowerPanel.mHeight * (0.5f - (0.5f / immersiveModeFactor));
        layer.subImage.imageRect.extent.width =
            (mLowerPanel.mWidth - cropHoriz) / immersiveModeFactor;
        layer.subImage.imageRect.extent.height = mLowerPanel.mHeight / immersiveModeFactor;
        layer.subImage.imageArrayIndex         = 0;
        layer.pose                             = mLowerPanel.mPanelFromWorld;
        const auto scale =
            GetDensityScaleForSize(mLowerPanel.mWidth - cropHoriz, -mLowerPanel.mHeight,
                                   mLowerPanel.mScaleFactor, mResolutionFactor);
        layer.size.width           = scale.x;
        layer.size.height          = scale.y;
        layers[layerCount++].mQuad = layer;
    }
}

bool GameSurfaceLayer::GetRayIntersectionWithPanelTopPanel(const XrVector3f& start,
                                                           const XrVector3f& end,
                                                           XrVector2f&       result2d,
                                                           XrPosef&          result3d) const

{
    const auto scale = GetDensityScaleForSize(mTopPanel.mWidth, mTopPanel.mHeight,
                                              mTopPanel.mScaleFactor, mResolutionFactor);
    return ::GetRayIntersectionWithPanel(mTopPanel, scale, start, end, result2d, result3d,
                                         mResolutionFactor);
}

bool GameSurfaceLayer::GetRayIntersectionWithPanel(const XrVector3f& start,
                                                   const XrVector3f& end,
                                                   XrVector2f&       result2d,
                                                   XrPosef&          result3d) const {
    const XrVector2f scale = GetDensityScaleForSize(mLowerPanel.mWidth, mLowerPanel.mHeight,
                                                    mLowerPanel.mScaleFactor, mResolutionFactor);
    return ::GetRayIntersectionWithPanel(mLowerPanel, scale, start, end, result2d, result3d,
                                         mResolutionFactor);
}

void GameSurfaceLayer::SetTopPanelFromController(const XrVector3f& controllerPosition) {

    static const XrVector3f viewerPosition{0, 0, 0}; // Set viewer position
    const float             sphereRadius = XrMath::Vector3f::Length(
                    mTopPanel.mPanelFromWorld.position - viewerPosition); // Set the initial distance of the

    // window from the viewer
    const XrVector3f windowUpDirection{0, 1, 0}; // Y is up

    const XrVector3f windowPosition =
        CalculatePanelPosition(viewerPosition, controllerPosition, sphereRadius);
    const XrQuaternionf windowRotation =
        CalculatePanelRotation(windowPosition, viewerPosition, windowUpDirection);
    if (windowPosition.y < 0) { return; }
    if (XrMath::Quatf::GetYawInRadians(windowRotation) > MATH_FLOAT_PI / 3.0f) { return; }

    mTopPanel.mPanelFromWorld = XrPosef{windowRotation, windowPosition};
}

XrPosef GameSurfaceLayer::GetTopPanelFromHeadPose(uint32_t eye, const XrPosef& headPose) {
    XrVector3f panelPosition = headPose.position;

    XrVector3f forward, up, right;
    XrMath::Quatf::ToVectors(headPose.orientation, forward, right, up);

    panelPosition.z += kSuperImmersiveRadius * (forward.x * 0.58f);
    panelPosition.y -= kSuperImmersiveRadius * (forward.z * 0.58f);
    panelPosition.x += kSuperImmersiveRadius * (forward.y * 0.58f);

    panelPosition.z += up.x / 25.f;
    panelPosition.y -= up.z / 25.f;
    panelPosition.x += up.y / 25.f;

    if (mImmersiveMode == 3) {
        panelPosition.z += right.x * (0.065f / 2.f) * (1 - 2.f * eye);
        panelPosition.y -= right.z * (0.065f / 2.f) * (1 - 2.f * eye);
        panelPosition.x += right.y * (0.065f / 2.f) * (1 - 2.f * eye);
    }

    return XrPosef{headPose.orientation, panelPosition};
}

// based on thumbstick, modify the depth of the top panel
void GameSurfaceLayer::SetTopPanelFromThumbstick(const float thumbstickY) {
    static constexpr float kDepthSpeed = 0.05f;
    static constexpr float kMaxDepth   = -10.0f;

    mTopPanel.mPanelFromWorld.position.z -= (thumbstickY * kDepthSpeed);
    mTopPanel.mPanelFromWorld.position.z =
        std::min(mTopPanel.mPanelFromWorld.position.z, mLowerPanel.mPanelFromWorld.position.z);
    mTopPanel.mPanelFromWorld.position.z =
        std::max(mTopPanel.mPanelFromWorld.position.z, kMaxDepth);
}

// Next error code: -2
int32_t GameSurfaceLayer::Init(const XrSession& session, const jobject activityObject) {
    if (mImmersiveMode > 0) {
        ALOGI("Using VR immersive mode {}", mImmersiveMode);
        mTopPanel.mPanelFromWorld.position.z   = mLowerPanel.mPanelFromWorld.position.z;
        mLowerPanel.mPanelFromWorld.position.y = -1.0f;
    }
    static const std::string gameSurfaceClassName = "org/citra/citra_emu/vr/GameSurfaceLayer";
    mVrGameSurfaceClass =
        JniUtils::GetGlobalClassReference(mEnv, activityObject, gameSurfaceClassName.c_str());
    BAIL_ON_COND(mVrGameSurfaceClass == nullptr, "No java Game Surface Layer class", -1);
    CreateSwapchain();
    SetSurface(activityObject);
    return 0;
}

void GameSurfaceLayer::Shutdown() {
    xrDestroySwapchain(mSwapchain.mHandle);
    mSwapchain.mHandle = XR_NULL_HANDLE;
    mSwapchain.mWidth  = 0;
    mSwapchain.mHeight = 0;
    mEnv->DeleteGlobalRef(mVrGameSurfaceClass);
}

void GameSurfaceLayer::CreateSwapchain() {
    // Initialize swapchain

    XrSwapchainCreateInfo xsci;
    memset(&xsci, 0, sizeof(xsci));
    xsci.type        = XR_TYPE_SWAPCHAIN_CREATE_INFO;
    xsci.next        = nullptr;
    xsci.usageFlags  = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    xsci.format      = 0;
    xsci.sampleCount = 0;
    xsci.width       = SURFACE_WIDTH_UNSCALED * mResolutionFactor;
    xsci.height      = SURFACE_HEIGHT_UNSCALED * mResolutionFactor;

    xsci.faceCount = 0;
    xsci.arraySize = 0;
    // Note: you can't have mips when you render directly to a
    // surface-backed swapchain. You just have to scale everything
    // so that you do not need them.
    xsci.mipCount = 0;

    ALOGI("GameSurfaceLayer: Creating swapchain of size {}x{} ({}x{} with "
          "resolution factor {}x)",
          xsci.width, xsci.height, SURFACE_WIDTH_UNSCALED, SURFACE_HEIGHT_UNSCALED,
          mResolutionFactor);

    PFN_xrCreateSwapchainAndroidSurfaceKHR pfnCreateSwapchainAndroidSurfaceKHR = nullptr;
    assert(OpenXr::GetInstance() != XR_NULL_HANDLE);
    XrResult xrResult =
        xrGetInstanceProcAddr(OpenXr::GetInstance(), "xrCreateSwapchainAndroidSurfaceKHR",
                              (PFN_xrVoidFunction*)(&pfnCreateSwapchainAndroidSurfaceKHR));
    if (xrResult != XR_SUCCESS || pfnCreateSwapchainAndroidSurfaceKHR == nullptr) {
        FAIL("xrGetInstanceProcAddr failed for "
             "xrCreateSwapchainAndroidSurfaceKHR");
    }

    OXR(pfnCreateSwapchainAndroidSurfaceKHR(mSession, &xsci, &mSwapchain.mHandle, &mSurface));
    mSwapchain.mWidth  = xsci.width;
    mSwapchain.mHeight = xsci.height;
}
