/***************************************************************************************************

Filename    :   UILayer.h

Content     :   Utility class for creating interactive Android UI windows that
                are displayed in the VR environment.

Authors     :   Amanda M. Watson
License     :   Licensed under GPLv3 or any later version.
                Refer to the license.txt file included.

***************************************************************************************************/

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
 * on the virtual Window.
 */
class AndroidWindowBounds {
public:
    AndroidWindowBounds() = default;
    AndroidWindowBounds(const float widthInDp, const float heightInDp)
        : mLeftInDp(0.0f)
        , mRightInDp(widthInDp)
        , mTopInDp(0.0f)
        , mBottomInDp(heightInDp) {}

    float Width() const { return mRightInDp - mLeftInDp; }
    float Height() const { return mBottomInDp - mTopInDp; }

    // "DP" refers to display pixels, which are the same as Android's
    // "density-independent pixels" (dp).
    // Note
    float mLeftInDp   = 0;
    float mRightInDp  = 0;
    float mTopInDp    = 0;
    float mBottomInDp = 0;

    // given a 2D point in worldspace 'point2d', returns the transformed
    // coordinate in DP, written to 'result'
    void Transform(const XrVector2f& point2d, XrVector2f& result) const {
        const float left = mLeftInDp;
        const float top  = mTopInDp;

        const float width  = Width();
        const float height = Height();

        result.x = (point2d.x * width) + left + (width / 2.0);
        // Android has a flipped vertical axis from OpenXR
        result.y = ((1.0 - point2d.y) * height) + top - (height / 2.0);
    }
};

struct BoundsHandle {
    BoundsHandle(AndroidWindowBounds* _p)
        : p(_p) {}
    BoundsHandle(jlong _l)
        : l(_l) {}

    union {
        AndroidWindowBounds* p = nullptr;
        jlong                l;
    };
};

//-----------------------------------------------------------------------------
// JNI functions

extern "C" JNIEXPORT void JNICALL
Java_org_citra_citra_1emu_vr_ui_VrUILayer_00024Companion_nativeSetBounds(JNIEnv* env, jobject thiz,
                                                                         jlong handle, jint left,
                                                                         jint top, jint right,
                                                                         jint bottom) {
    AndroidWindowBounds* b = BoundsHandle(handle).p;
    b->mLeftInDp           = left;
    b->mTopInDp            = top;
    b->mRightInDp          = right;
    b->mBottomInDp         = bottom;
}

//-----------------------------------------------------------------------------
// Local sysprops

static constexpr std::chrono::milliseconds kMinTimeBetweenChecks(500);

// Get density on an interval
float GetDensitySysprop() {
    const float                                               kDefaultDensity = 1200;
    static float                                              lastDensity     = kDefaultDensity;
    static std::chrono::time_point<std::chrono::steady_clock> lastTime        = {};
    // Only check the sysprop every 500ms
    if ((std::chrono::steady_clock::now() - lastTime) >= kMinTimeBetweenChecks) {
        lastTime    = std::chrono::steady_clock::now();
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
    const XrPosef    worldFromPanel = XrMath::Posef::Inverted(panelFromWorld);
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
    result3d.orientation = panelFromWorld.orientation;

    const XrVector2f result2dNDC = {
        (localStart.x + (localEnd.x - localStart.x) * tan) / (scaleFactor.x),
        (localStart.y + (localEnd.y - localStart.y) * tan) / (scaleFactor.y)};

    const AndroidWindowBounds viewBounds(panelWidth, panelHeight);
    viewBounds.Transform(result2dNDC, result2d);
    const bool isInBounds = result2d.x >= 0 && result2d.y >= 0 && result2d.x < viewBounds.Width() &&
                            result2d.y < viewBounds.Height();
    if (!isInBounds) { return false; }

    return true;
}

// Uses a density for scaling and sets aspect ratio
XrVector2f GetDensityScaleForSize(const int32_t texWidth, const int32_t texHeight,
                                  const float scaleFactor) {
    const float density = GetDensitySysprop();
    return XrVector2f{static_cast<float>(texWidth) / density,
                      (static_cast<float>(texHeight) / density)} *
           scaleFactor;
}

} // anonymous namespace

UILayer::UILayer(const std::string& className, const XrVector3f&& position,
                 const XrQuaternionf&& orientation, JNIEnv* env, jobject activityObject,
                 const XrSession& session)
    : mSession(session)
    , mPanelFromWorld(XrPosef{XrMath::Quatf::Identity(), position})
    , mEnv(env) {
    const int32_t initializationStatus = Init(className, activityObject, position, session);
    if (initializationStatus < 0) {
        FAIL("Could not initialize UILayer(%s) -- error '%d'", className.c_str(),
             initializationStatus);
    }
}

UILayer::~UILayer() { Shutdown(); }

void UILayer::Frame(const XrSpace&                   space,
                    std::vector<XrCompositionLayer>& layers,
                    uint32_t&                        layerCount) const

{

    XrCompositionLayerQuad layer = {};

    layer.type       = XR_TYPE_COMPOSITION_LAYER_QUAD;
    layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    layer.layerFlags |= XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
    layer.layerFlags |= XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
    // NOTE: may not want unpremultiplied alpha

    layer.space = space;

    layer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
    memset(&layer.subImage, 0, sizeof(XrSwapchainSubImage));
    layer.subImage.swapchain               = mSwapchain.mHandle;
    layer.subImage.imageRect.offset.x      = 0;
    layer.subImage.imageRect.offset.y      = 0;
    layer.subImage.imageRect.extent.width  = mSwapchain.mWidth;
    layer.subImage.imageRect.extent.height = mSwapchain.mHeight;
    layer.subImage.imageArrayIndex         = 0;
    layer.pose                             = mPanelFromWorld;
    const auto scale  = GetDensityScaleForSize(mSwapchain.mWidth, -mSwapchain.mHeight, 1.0f);
    layer.size.width  = scale.x;
    layer.size.height = scale.y;
    layers[layerCount++].mQuad = layer;
}

bool UILayer::GetRayIntersectionWithPanel(const XrVector3f& start,
                                          const XrVector3f& end,
                                          XrVector2f&       result2d,
                                          XrPosef&          result3d) const {
    const XrVector2f scale = GetDensityScaleForSize(mSwapchain.mWidth, mSwapchain.mHeight, 1.0f);
    return ::GetRayIntersectionWithPanel(mPanelFromWorld, mSwapchain.mWidth, mSwapchain.mHeight,
                                         scale, start, end, result2d, result3d);
}

// Next error code: -8
int32_t UILayer::Init(const std::string& className, const jobject activityObject,
                      const XrVector3f& position, const XrSession& session) {
    mVrUILayerClass = JniUtils::GetGlobalClassReference(mEnv, activityObject, className.c_str());
    BAIL_ON_COND(mVrUILayerClass == nullptr, "No java UI Layer class", -1);

    jmethodID vrUILayerConstructor =
        mEnv->GetMethodID(mVrUILayerClass, "<init>", "(Lorg/citra/citra_emu/vr/VrActivity;)V");
    BAIL_ON_COND(vrUILayerConstructor == nullptr, "no java window constructor", -2);

    mVrUILayerObject = mEnv->NewObject(mVrUILayerClass, vrUILayerConstructor, activityObject);
    BAIL_ON_COND(mVrUILayerObject == nullptr, "Could not construct java window", -3);

    mGetBoundsMethodID = mEnv->GetMethodID(mVrUILayerClass, "getBoundsForView", "(J)I");
    BAIL_ON_COND(mGetBoundsMethodID == nullptr, "could not find getBoundsForView()", -4);

    mSetSurfaceMethodId =
        mEnv->GetMethodID(mVrUILayerClass, "setSurface", "(Landroid/view/Surface;II)I");
    BAIL_ON_COND(mSetSurfaceMethodId == nullptr, "could not find setSurface()", -5);

    mSendClickToUIMethodID = mEnv->GetMethodID(mVrUILayerClass, "sendClickToUI", "(FFI)I");

    BAIL_ON_COND(mSendClickToUIMethodID == nullptr, "could not find sendClickToUI()", -6);

    BAIL_ON_ERR(CreateSwapchain(), -7);

    return 0;
}

void UILayer::Shutdown() {
    xrDestroySwapchain(mSwapchain.mHandle);
    mSwapchain.mHandle = XR_NULL_HANDLE;
    mSwapchain.mWidth  = 0;
    mSwapchain.mHeight = 0;
    mEnv->DeleteGlobalRef(mVrUILayerClass);
    mVrUILayerClass = nullptr;
    // These steps are not strictly necessary for app shutdown, as references are cleaned up when
    // the JVM is destroyed, but memory-saving if this class is destroyed/re-initialized at runtime.
    mEnv->DeleteLocalRef(mVrUILayerObject);
    mVrUILayerObject = nullptr;
}

int UILayer::CreateSwapchain() {
    {
        AndroidWindowBounds viewBounds;
        if (mEnv->ExceptionCheck()) { mEnv->ExceptionClear(); }
        const jint ret =
            mEnv->CallIntMethod(mVrUILayerObject, mGetBoundsMethodID, BoundsHandle(&viewBounds).l);
        // Check for exceptions (and log them).
        if (mEnv->ExceptionCheck()) {
            mEnv->ExceptionDescribe();
            mEnv->ExceptionClear();
            FAIL("Exception in getBoundsForView()");
        }
        if (ret < 0) {
            ALOGE("{}() returned error {}", __FUNCTION__, ret);
            return -1;
        }
        if (viewBounds.Width() == 0 || viewBounds.Height() == 0) {
            ALOGE("{}() returned invalid bounds {} x {}", __FUNCTION__, viewBounds.Width(),
                  viewBounds.Height());
            return -2;
        }
        mSwapchain.mWidth  = viewBounds.Width();
        mSwapchain.mHeight = viewBounds.Height();
    }
    // Initialize swapchain.
    {
        XrSwapchainCreateInfo swapchainCreateInfo;
        memset(&swapchainCreateInfo, 0, sizeof(swapchainCreateInfo));
        swapchainCreateInfo.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
        swapchainCreateInfo.next = nullptr;
        swapchainCreateInfo.usageFlags =
            XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        swapchainCreateInfo.format      = 0;
        swapchainCreateInfo.sampleCount = 0;
        swapchainCreateInfo.width       = mSwapchain.mWidth;
        swapchainCreateInfo.height      = mSwapchain.mHeight;
        swapchainCreateInfo.faceCount   = 0;
        swapchainCreateInfo.arraySize   = 0;
        swapchainCreateInfo.mipCount    = 0;

        PFN_xrCreateSwapchainAndroidSurfaceKHR pfnCreateSwapchainAndroidSurfaceKHR = nullptr;
        assert(OpenXr::GetInstance() != XR_NULL_HANDLE);
        XrResult xrResult =
            xrGetInstanceProcAddr(OpenXr::GetInstance(), "xrCreateSwapchainAndroidSurfaceKHR",
                                  (PFN_xrVoidFunction*)(&pfnCreateSwapchainAndroidSurfaceKHR));
        if (xrResult != XR_SUCCESS || pfnCreateSwapchainAndroidSurfaceKHR == nullptr) {
            FAIL("xrGetInstanceProcAddr failed for "
                 "xrCreateSwapchainAndroidSurfaceKHR");
        }

        OXR(pfnCreateSwapchainAndroidSurfaceKHR(mSession, &swapchainCreateInfo, &mSwapchain.mHandle,
                                                &mSurface));

        ALOGD("UILayer: created swapchain {}x{}", mSwapchain.mWidth, mSwapchain.mHeight);

        mEnv->CallIntMethod(mVrUILayerObject, mSetSurfaceMethodId, mSurface, (int)mSwapchain.mWidth,
                            (int)mSwapchain.mHeight);
        if (mEnv->ExceptionCheck()) {
            mEnv->ExceptionDescribe();
            mEnv->ExceptionClear();
            FAIL("Exception in setSurface()");
        }
    }
    return 0;
}

void UILayer::SendClickToUI(const XrVector2f& pos2d, const int type) {
    mEnv->CallIntMethod(mVrUILayerObject, mSendClickToUIMethodID, pos2d.x, pos2d.y, type);
}
