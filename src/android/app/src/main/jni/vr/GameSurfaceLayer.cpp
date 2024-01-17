/*******************************************************************************

Filename    :   GameSurfaceLayer.cpp

Content     :   Handles the projection of the "Game Surface" panels into XR.
                Includes the "top" panel (stereo game screen) and the "bottom"
                panel (mono touchpad).

Authors     :   Amanda M. Watson
License     :   Licensed under GPLv2 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#include "GameSurfaceLayer.h"

#include "utils/JniUtils.h"
#include "utils/LogUtils.h"
#include "utils/SyspropUtils.h"
#include "utils/XrMath.h"

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <chrono>

#include <stdlib.h>

namespace {

constexpr float kLowerPanelScaleFactor = 0.75f;
/** Used to translate texture coordinates into the corresponding coordinates
 * on the Android Activity Window.
 *
 * EmulationActivity still thinks its window
 * (invisible) shows the game surface, so when we forward touch events to
 * corresponding coordinates on the window, it will be as if the user touched
 * the game surface.
 */
class AndroidWindowSpace {
public:
    AndroidWindowSpace(const float widthInDp, const float heightInDp)
        : widthInDp_(widthInDp), heightInDp_(heightInDp) {}
    float Width() const {
        return widthInDp_;
    }
    float Height() const {
        return heightInDp_;
    }

    // "DP" refers to display pixels, which are the same as Android's
    // "density-independent pixels" (dp).
    const float widthInDp_ = 0;
    const float heightInDp_ = 0;

    // given a 2D point in worldspace 'point2d', returns the transformed
    // coordinate in DP, written to 'result'
    void Transform(const XrVector2f& point2d, XrVector2f& result) const {
        const float width = static_cast<float>(Width());
        const float height = static_cast<float>(Height());

        result.x = (point2d.x * width) + (width / 2.0f);
        // Android has a flipped vertical axis from OpenXR
        result.y = ((1.0f - point2d.y) * height) - (height / 2.0f);
    }
};

//-----------------------------------------------------------------------------
// Local sysprops

static constexpr std::chrono::milliseconds kMinTimeBetweenChecks(500);

// Get density on an interval
float GetDensitySysprop() {
    static constexpr float kDefaultDensity = GameSurfaceLayer::DEFAULT_QUAD_DENSITY;
    static float lastDensity = kDefaultDensity;
    static std::chrono::time_point<std::chrono::steady_clock> lastTime = {};
    // Only check the sysprop every 500ms
    if ((std::chrono::steady_clock::now() - lastTime) >= kMinTimeBetweenChecks) {
        lastTime = std::chrono::steady_clock::now();
        lastDensity = SyspropUtils::GetSysPropAsFloat("debug.citra.density", kDefaultDensity);
    }
    return lastDensity;
}

int32_t GetCylinderSysprop() {
    static constexpr int32_t kDefaultCylinder = 0;
    static int32_t lastCylinder = kDefaultCylinder;
    static std::chrono::time_point<std::chrono::steady_clock> lastTime = {};
    // Only check the sysprop every 500ms
    if ((std::chrono::steady_clock::now() - lastTime) >= kMinTimeBetweenChecks) {
        lastTime = std::chrono::steady_clock::now();
        lastCylinder = SyspropUtils::GetSysPropAsInt("debug.citra.cylinder", kDefaultCylinder);
    }
    return lastCylinder;
}

float GetRadiusSysprop() {
    static constexpr float kDefaultRadius = GameSurfaceLayer::DEFAULT_CYLINDER_RADIUS;
    static float lastRadius = kDefaultRadius;
    static std::chrono::time_point<std::chrono::steady_clock> lastTime = {};
    // Only check the sysprop every 500ms
    if ((std::chrono::steady_clock::now() - lastTime) >= kMinTimeBetweenChecks) {
        lastTime = std::chrono::steady_clock::now();
        lastRadius = SyspropUtils::GetSysPropAsFloat("debug.citra.radius", kDefaultRadius);
    }
    return lastRadius;
}

float GetCentralAngleSysprop() {
    static constexpr float kDefaultCentralAngle =
        GameSurfaceLayer::DEFAULT_CYLINDER_CENTRAL_ANGLE_DEGREES;
    static float lastCentralAngle = kDefaultCentralAngle;
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

/** Create the pose for the lower panel.
 *    - Below the center
 *    - Rotated 45 degrees away from viewer
 *    - Half the distance away from viewer as top panel
 */
XrPosef CreateLowerPanelFromWorld(const XrPosef& topPanelFromWorld) {

    constexpr float kLowerPanelYOffset = -0.75f;
    XrPosef lowerPanelFromWorld = topPanelFromWorld;
    // Pitch the lower panel away from the viewer 45 degrees
    lowerPanelFromWorld.orientation = XrMath::Quatf::FromEuler(0.0f, -MATH_FLOAT_PI / 4.0f, 0.0f);
    lowerPanelFromWorld.position.y += kLowerPanelYOffset;
    lowerPanelFromWorld.position.z = -1.5f;
    return lowerPanelFromWorld;
}

XrVector3f CalculatePanelPosition(const XrVector3f& viewerPosition,
                                  const XrVector3f& controllerPosition, float sphereRadius) {
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

bool GetRayIntersectionWithPanel(const XrPosef& panelFromWorld, const uint32_t panelWidth,
                                 const uint32_t panelHeight, const XrVector2f& scaleFactor,
                                 const XrVector3f& start, const XrVector3f& end,
                                 XrVector2f& result2d, XrPosef& result3d)

{
    const XrPosef worldFromPanel = XrMath::Posef::Inverted(panelFromWorld);
    const XrVector3f localStart = XrMath::Posef::Transform(worldFromPanel, start);
    const XrVector3f localEnd = XrMath::Posef::Transform(worldFromPanel, end);
    // Note: assumes layer lies in the XZ plane
    const float tan = localStart.z / (localStart.z - localEnd.z);

    // Check for backwards controller
    if (tan < 0) {
        ALOGD("Backwards controller");
        return false;
    }
    result3d.position = start + (end - start) * tan;
    result3d.orientation = panelFromWorld.orientation;

    const XrVector2f result2dNDC = {
        (localStart.x + (localEnd.x - localStart.x) * tan) / (scaleFactor.x),
        (localStart.y + (localEnd.y - localStart.y) * tan) / (scaleFactor.y)};

    const AndroidWindowSpace androidSpace(panelWidth, panelHeight);
    androidSpace.Transform(result2dNDC, result2d);
    const bool isInBounds = result2d.x >= 0 && result2d.y >= 0 &&
                            result2d.x < androidSpace.Width() && result2d.y < androidSpace.Height();
    result2d.y += androidSpace.Height();

    if (!isInBounds) {
        return false;
    }

    return true;
}

// Uses a density for scaling and sets aspect ratio
XrVector2f GetDensityScaleForSize(const int32_t texWidth, const int32_t texHeight,
                                  const float scaleFactor) {
    const float density = GetDensitySysprop();
    return XrVector2f{2.0f * static_cast<float>(texWidth) / density,
                      (static_cast<float>(texHeight) / density)} *
           scaleFactor;
}

XrPosef CreateTopPanelFromWorld(const XrVector3f& position) {
    return XrPosef{XrMath::Quatf::Identity(), position};
}

} // anonymous namespace

GameSurfaceLayer::GameSurfaceLayer(const XrVector3f&& position, JNIEnv* env, jobject activityObject,
                                   const XrSession& session)
    : session_(session), topPanelFromWorld_(CreateTopPanelFromWorld(position)),
      lowerPanelFromWorld_(CreateLowerPanelFromWorld(topPanelFromWorld_)), env_(env),
      activityObject_(activityObject)

{
    const int32_t initializationStatus = Init(activityObject, position, session);
    if (initializationStatus < 0) {
        FAIL("Could not initialize GameSurfaceLayer -- error '%d'", initializationStatus);
    }
}

GameSurfaceLayer::~GameSurfaceLayer() {
    Shutdown();
}

void GameSurfaceLayer::SetSurface() const {
    assert(vrGameSurfaceClass_ != nullptr);
    assert(setSurfaceMethodID_ != nullptr);
    env_->CallStaticVoidMethod(vrGameSurfaceClass_, setSurfaceMethodID_, activityObject_, surface_);
}

void GameSurfaceLayer::Frame(const XrSpace& space, std::vector<XrCompositionLayer>& layers,
                             uint32_t& layerCount) const

{
    const uint32_t panelWidth = swapchain_.Width / 2;
    const uint32_t panelHeight = swapchain_.Height / 2;
    const double aspectRatio =
        static_cast<double>(2 * panelWidth) / static_cast<double>(panelHeight);
    // Prevent a seam between the top and bottom view
    constexpr uint32_t verticalBorderTex = 1;
    const int32_t useCylinder = GetCylinderSysprop();
    if (useCylinder == 1) {

        // Create the Top Display Panel (Curved display)
        for (uint32_t eye = 0; eye < NUM_EYES; eye++) {
            XrCompositionLayerCylinderKHR layer = {};

            layer.type = XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR;
            layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            layer.layerFlags |= XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
            layer.layerFlags |= XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
            // NOTE: may not want unpremultiplied alpha

            layer.space = space;

            layer.eyeVisibility = eye == 0 ? XR_EYE_VISIBILITY_LEFT : XR_EYE_VISIBILITY_RIGHT;
            memset(&layer.subImage, 0, sizeof(XrSwapchainSubImage));
            layer.subImage.swapchain = swapchain_.Handle;
            layer.subImage.imageRect.offset.x = eye == 0 ? 0 : panelWidth;
            layer.subImage.imageRect.offset.y = 0;
            layer.subImage.imageRect.extent.width = panelWidth;
            layer.subImage.imageRect.extent.height = panelHeight - verticalBorderTex;
            layer.subImage.imageArrayIndex = 0;
            layer.pose = topPanelFromWorld_;
            layer.pose.position.z += GetRadiusSysprop();

            // Radius effectively controls the width of the cylinder shape.
            // Central angle controls how much of the cylinder is
            // covered by the texture. Together, they control the
            // scale of the texture.
            const float radius = GetRadiusSysprop();
            layer.radius = radius;
            layer.centralAngle = GetCentralAngleSysprop() * MATH_FLOAT_PI / 180.0f;
            layer.aspectRatio = -aspectRatio;
            layers[layerCount++].mCylinder = layer;
        }
    } else {
        // Create the Top Display Panel (Flat display)
        for (uint32_t eye = 0; eye < 2; eye++) {
            XrCompositionLayerQuad layer = {};

            layer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
            layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            layer.layerFlags |= XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
            layer.layerFlags |= XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
            // NOTE: may not want unpremultiplied alpha

            layer.space = space;

            layer.eyeVisibility = eye == 0 ? XR_EYE_VISIBILITY_LEFT : XR_EYE_VISIBILITY_RIGHT;
            memset(&layer.subImage, 0, sizeof(XrSwapchainSubImage));
            layer.subImage.swapchain = swapchain_.Handle;
            layer.subImage.imageRect.offset.x = (eye == 0 ? 0 : panelWidth) + 125;
            layer.subImage.imageRect.offset.y = 0;
            layer.subImage.imageRect.extent.width = panelWidth - 250;
            layer.subImage.imageRect.extent.height = panelHeight - verticalBorderTex;
            layer.subImage.imageArrayIndex = 0;
            layer.pose = topPanelFromWorld_;
            // Scale to get the desired density within the visible area (if we
            // want).
            const auto scale = GetDensityScaleForSize(panelWidth - 250, -panelHeight, 1.0f);
            layer.size.width = scale.x;
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
    {
        XrCompositionLayerQuad layer = {};

        layer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
        layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        layer.layerFlags |= XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
        layer.layerFlags |= XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
        // NOTE: may not want unpremultiplied alpha

        layer.space = space;

        layer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
        memset(&layer.subImage, 0, sizeof(XrSwapchainSubImage));
        layer.subImage.swapchain = swapchain_.Handle;
        layer.subImage.imageRect.offset.x = 225;
        layer.subImage.imageRect.offset.y = panelHeight + verticalBorderTex;
        layer.subImage.imageRect.extent.width = panelWidth - 450;
        layer.subImage.imageRect.extent.height = panelHeight;
        layer.subImage.imageArrayIndex = 0;
        layer.pose = lowerPanelFromWorld_;
        const auto scale =
            GetDensityScaleForSize(panelWidth - 450, -panelHeight, kLowerPanelScaleFactor);
        layer.size.width = scale.x;
        layer.size.height = scale.y;
        layers[layerCount++].mQuad = layer;
    }
}

bool GameSurfaceLayer::GetRayIntersectionWithPanelTopPanel(const XrVector3f& start,
                                                           const XrVector3f& end,
                                                           XrVector2f& result2d,
                                                           XrPosef& result3d) const

{
    const uint32_t panelWidth = swapchain_.Width / 2;
    const uint32_t panelHeight = swapchain_.Height / 2;
    const auto scale = GetDensityScaleForSize(panelWidth, panelHeight, 1.0f);
    return ::GetRayIntersectionWithPanel(topPanelFromWorld_, panelWidth, panelHeight, scale, start,
                                         end, result2d, result3d);
}

bool GameSurfaceLayer::GetRayIntersectionWithPanel(const XrVector3f& start, const XrVector3f& end,
                                                   XrVector2f& result2d, XrPosef& result3d) const {
    const uint32_t panelWidth = swapchain_.Width / 2;
    const uint32_t panelHeight = swapchain_.Height / 2;
    const XrVector2f scale =
        GetDensityScaleForSize(panelWidth, panelHeight, kLowerPanelScaleFactor);
    return ::GetRayIntersectionWithPanel(lowerPanelFromWorld_, panelWidth, panelHeight, scale,
                                         start, end, result2d, result3d);
}

void GameSurfaceLayer::SetTopPanelFromController(const XrVector3f& controllerPosition) {

    static const XrVector3f viewerPosition{0, 0, 0}; // Set viewer position
    const float sphereRadius = XrMath::Vector3f::Length(
        topPanelFromWorld_.position - viewerPosition); // Set the initial distance of the

    // window from the viewer
    const XrVector3f windowUpDirection{0, 1, 0}; // Y is up

    const XrVector3f windowPosition =
        CalculatePanelPosition(viewerPosition, controllerPosition, sphereRadius);
    const XrQuaternionf windowRotation =
        CalculatePanelRotation(windowPosition, viewerPosition, windowUpDirection);
    if (windowPosition.y < 0) {
        return;
    }
    if (XrMath::Quatf::GetYawInRadians(windowRotation) > MATH_FLOAT_PI / 3.0f) {
        return;
    }

    topPanelFromWorld_ = XrPosef{windowRotation, windowPosition};
}

// based on thumbstick, modify the depth of the top panel
void GameSurfaceLayer::SetTopPanelFromThumbstick(const float thumbstickY) {
    static constexpr float kDepthSpeed = 0.05f;
    static constexpr float kMaxDepth = -10.0f;

    topPanelFromWorld_.position.z -= (thumbstickY * kDepthSpeed);
    topPanelFromWorld_.position.z =
        std::min(topPanelFromWorld_.position.z, lowerPanelFromWorld_.position.z);
    topPanelFromWorld_.position.z = std::max(topPanelFromWorld_.position.z, kMaxDepth);
}

// Next error code: -2
int32_t GameSurfaceLayer::Init(const jobject activityObject, const XrVector3f& position,
                               const XrSession& session) {

    static const std::string gameSurfaceClassName = "org/citra/citra_emu/vr/GameSurfaceLayer";
    vrGameSurfaceClass_ =
        JniUtils::GetGlobalClassReference(env_, activityObject, gameSurfaceClassName);
    BAIL_ON_COND(vrGameSurfaceClass_ == nullptr, "No java Game Surface Layer class", -1);

    CreateSwapchain();
    SetSurface();

    return 0;
}

void GameSurfaceLayer::Shutdown() {
    xrDestroySwapchain(swapchain_.Handle);
    // This currently causes a memory exception
    //    env_->DeleteGlobalRef(vrGameSurfaceClass_);
}

void GameSurfaceLayer::CreateSwapchain() {
    // Initialize swapchain
    {
        XrSwapchainCreateInfo xsci;
        memset(&xsci, 0, sizeof(xsci));
        xsci.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
        xsci.next = nullptr;
        xsci.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        xsci.format = 0;
        xsci.sampleCount = 0;
        xsci.width = SURFACE_WIDTH;
        xsci.height = SURFACE_HEIGHT;

        xsci.faceCount = 0;
        xsci.arraySize = 0;
        // Note: you can't have mips when you render directly to a
        // surface-backed swapchain. You just have to scale everything
        // so that you do not need them.
        xsci.mipCount = 0;

        PFN_xrCreateSwapchainAndroidSurfaceKHR pfnCreateSwapchainAndroidSurfaceKHR = nullptr;
        assert(OpenXr::GetInstance() != XR_NULL_HANDLE);
        XrResult xrResult =
            xrGetInstanceProcAddr(OpenXr::GetInstance(), "xrCreateSwapchainAndroidSurfaceKHR",
                                  (PFN_xrVoidFunction*)(&pfnCreateSwapchainAndroidSurfaceKHR));
        if (xrResult != XR_SUCCESS || pfnCreateSwapchainAndroidSurfaceKHR == nullptr) {
            FAIL("xrGetInstanceProcAddr failed for "
                 "xrCreateSwapchainAndroidSurfaceKHR");
        }

        OXR(pfnCreateSwapchainAndroidSurfaceKHR(session_, &xsci, &swapchain_.Handle, &surface_));
        swapchain_.Width = xsci.width;
        swapchain_.Height = xsci.height;
    }

    setSurfaceMethodID_ =
        env_->GetStaticMethodID(vrGameSurfaceClass_, "setSurface",
                                "(Lorg/citra/citra_emu/vr/VrActivity;Landroid/view/Surface;)V");
    if (setSurfaceMethodID_ == nullptr) {
        FAIL("Couldn't find setSurface()");
    }
}
