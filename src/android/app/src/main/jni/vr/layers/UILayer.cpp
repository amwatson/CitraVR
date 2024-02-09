/*******************************************************************************

Filename    :   UILayer.cpp

Content     :   Handles the projection of the "Game Surface" panels into XR.
                Includes the "top" panel (stereo game screen) and the "bottom"
                panel (mono touchpad).

Authors     :   Amanda M. Watson
License     :   Licensed under GPLv3 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#include "UILayer.h"

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
        : leftInDp_(0.0f), rightInDp_(widthInDp), topInDp_(0.0f), bottomInDp_(heightInDp) {}

    float Width() const {
        return rightInDp_ - leftInDp_;
    }
    float Height() const {
        return bottomInDp_ - topInDp_;
    }

    // "DP" refers to display pixels, which are the same as Android's
    // "density-independent pixels" (dp).
    float leftInDp_ = 0;
    float rightInDp_ = 0;
    float topInDp_ = 0;
    float bottomInDp_ = 0;

    // given a 2D point in worldspace 'point2d', returns the transformed
    // coordinate in DP, written to 'result'
    void Transform(const XrVector2f& point2d, XrVector2f& result) const {
        const float left = leftInDp_;
        const float top = topInDp_;

        const float width = Width();
        const float height = Height();

        result.x = (point2d.x * width) + left + (width / 2.0);
        // Android has a flipped vertical axis from OpenXR
        result.y = ((1.0 - point2d.y) * height) + top - (height / 2.0);
    }
};

//-----------------------------------------------------------------------------
// Local sysprops

static constexpr std::chrono::milliseconds kMinTimeBetweenChecks(500);

// Get density on an interval
float GetDensitySysprop() {
    const float kDefaultDensity = 1200;
    static float lastDensity = kDefaultDensity;
    static std::chrono::time_point<std::chrono::steady_clock> lastTime = {};
    // Only check the sysprop every 500ms
    if ((std::chrono::steady_clock::now() - lastTime) >= kMinTimeBetweenChecks) {
        lastTime = std::chrono::steady_clock::now();
        lastDensity = SyspropUtils::GetSysPropAsFloat("debug.citra.density", kDefaultDensity);
    }
    return lastDensity;
}

//-----------------------------------------------------------------------------
// Panel math

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

XrPosef CreatePanelFromWorld(const XrVector3f& position) {
    return XrPosef{XrMath::Quatf::Identity(), position};
}

} // anonymous namespace

UILayer::UILayer(const std::string& className, const XrVector3f&& position, JNIEnv* env,
                 jobject activityObject, const XrSession& session)
    : session_(session), panelFromWorld_(CreatePanelFromWorld(position)), env_(env),
      activityObject_(activityObject)

{
    const int32_t initializationStatus = Init(className, activityObject, position, session);
    if (initializationStatus < 0) {
        FAIL("Could not initialize UILayer -- error '%d'", initializationStatus);
    }
}

UILayer::~UILayer() {
    Shutdown();
}

void UILayer::Frame(const XrSpace& space, std::vector<XrCompositionLayer>& layers,
                    uint32_t& layerCount) const

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
    layer.subImage.imageRect.offset.x = 0;
    layer.subImage.imageRect.offset.y = 0;
    layer.subImage.imageRect.extent.width = swapchain_.Width;
    layer.subImage.imageRect.extent.height = swapchain_.Height;
    layer.subImage.imageArrayIndex = 0;
    layer.pose = panelFromWorld_;
    const auto scale = GetDensityScaleForSize(swapchain_.Width, swapchain_.Height, 1.0f);
    layer.size.width = scale.x;
    layer.size.height = scale.y;
    layers[layerCount++].mQuad = layer;
}

bool UILayer::GetRayIntersectionWithPanel(const XrVector3f& start, const XrVector3f& end,
                                          XrVector2f& result2d, XrPosef& result3d) const {
    const XrVector2f scale = GetDensityScaleForSize(swapchain_.Width, swapchain_.Height, 1.0f);
    return ::GetRayIntersectionWithPanel(panelFromWorld_, swapchain_.Width, swapchain_.Height,
                                         scale, start, end, result2d, result3d);
}

// Next error code: -4
int32_t UILayer::Init(const std::string& className, const jobject activityObject,
                      const XrVector3f& position, const XrSession& session) {
    vrUILayerClass_ = JniUtils::GetGlobalClassReference(env_, activityObject, className.c_str());
    BAIL_ON_COND(vrUILayerClass_ == nullptr, "No java UI Layer class", -1);

    jmethodID vrUILayerConstructor =
        env_->GetMethodID(vrUILayerClass_, "<init>", "(Lorg/citra/citra_emu/vr/VrActivity;)V");
    BAIL_ON_COND(vrUILayerConstructor == nullptr, "no java window constructor", -2);

    vrUILayerObject_ = env_->NewObject(vrUILayerClass_, vrUILayerConstructor, activityObject);
    BAIL_ON_COND(vrUILayerObject_ == nullptr, "Could not construct java window", -3);

    CreateSwapchain();
    return 0;
}

void UILayer::Shutdown() {
    xrDestroySwapchain(swapchain_.Handle);
    // This currently causes a memory exception
    //    env_->DeleteGlobalRef(vrUILayerClass_);
}

void UILayer::CreateSwapchain() {
    // Initialize swapchain

    XrSwapchainCreateInfo xsci;
    memset(&xsci, 0, sizeof(xsci));
    xsci.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
    xsci.next = nullptr;
    xsci.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    xsci.format = 0;
    xsci.sampleCount = 0;
    xsci.width = 500;
    xsci.height = 500;

    xsci.faceCount = 0;
    xsci.arraySize = 0;
    // Note: you can't have mips when you render directly to a
    // surface-backed swapchain. You just have to scale everything
    // so that you do not need them.
    xsci.mipCount = 0;

    ALOGI("UILayer: Creating swapchain of size {}x{}", xsci.width, xsci.height);

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

void UILayer::SendClickToUI(const XrVector2f& pos2d, const int type)
{
    env_->CallIntMethod(vrUILayerObject_, sendClickToWindowMethodID_, pos2d.x,
                        pos2d.y, type);
}
