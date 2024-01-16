/*******************************************************************************

Filename    :   OpenXR.cpp

Content     :   OpenXR initialization and shutdown code.

Authors     :   Amanda M. Watson
License     :   Licensed under GPLv2 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#include "OpenXR.h"

#include "utils/LogUtils.h"

#include <openxr/openxr_platform.h>

#include <vector>

#include <assert.h>

#define BAIL_ON_ERR(fn, returnCode)                                            \
    do {                                                                       \
        const int32_t ret = fn;                                                \
        if (ret < 0)                                                           \
        {                                                                      \
            ALOGE("ERROR (%s): %s() returned %d", __FUNCTION__, #fn, ret);     \
            return (returnCode);                                               \
        }                                                                      \
    } while (0)

XrInstance instance;
void OXR_CheckErrors(XrResult result, const char* function, bool failOnError)
{
    if (XR_FAILED(result))
    {
        char errorBuffer[XR_MAX_RESULT_STRING_SIZE];
        xrResultToString(instance, result, errorBuffer);
        if (failOnError)
        {
            FAIL("OpenXR error: %s: %s\n", function, errorBuffer);
        }
        else { ALOGV("OpenXR error: %s: %s\n", function, errorBuffer); }
    }
}

/*
   ================================================================================

   OpenXR Utility Functions

   ================================================================================
   */
namespace
{
#define DECL_PFN(pfn) PFN_##pfn pfn = nullptr
#define INIT_PFN(pfn)                                                          \
    OXR(xrGetInstanceProcAddr(instance, #pfn, (PFN_xrVoidFunction*)(&pfn)))

DECL_PFN(xrEnumerateDisplayRefreshRatesFB);
DECL_PFN(xrRequestDisplayRefreshRateFB);

void InitOXRFunctions(const XrInstance xrInstance)
{
    assert(xrInstance != XR_NULL_HANDLE);
    INIT_PFN(xrEnumerateDisplayRefreshRatesFB);
    INIT_PFN(xrRequestDisplayRefreshRateFB);
}

[[maybe_unused]] void XrEnumerateLayerProperties()
{
    XrResult                          result;
    PFN_xrEnumerateApiLayerProperties xrEnumerateApiLayerProperties;
    OXR(result = xrGetInstanceProcAddr(
            XR_NULL_HANDLE, "xrEnumerateApiLayerProperties",
            (PFN_xrVoidFunction*)&xrEnumerateApiLayerProperties));
    if (result != XR_SUCCESS)
    {
        FAIL("Failed to get xrEnumerateApiLayerProperties function pointer.");
    }

    uint32_t numInputLayers  = 0;
    uint32_t numOutputLayers = 0;
    OXR(xrEnumerateApiLayerProperties(numInputLayers, &numOutputLayers, NULL));

    numInputLayers = numOutputLayers;

    auto layerProperties = std::vector<XrApiLayerProperties>(numOutputLayers);

    for (auto& lp : layerProperties)
    {
        lp.type = XR_TYPE_API_LAYER_PROPERTIES;
        lp.next = NULL;
    }

    OXR(xrEnumerateApiLayerProperties(numInputLayers, &numOutputLayers,
                                      layerProperties.data()));

    for (uint32_t i = 0; i < numOutputLayers; i++)
    {
        ALOGI("Found layer %s", layerProperties[i].layerName);
    }
}

// Next return code: -3
int XrCheckRequiredExtensions(const char* const* requiredExtensionNames,
                              const size_t       numRequiredExtensions)
{

#ifndef NDEBUG
    XrEnumerateLayerProperties();
#endif

    // Check the list of required extensions against what is supported by the
    // runtime.
    {
        XrResult result;
        PFN_xrEnumerateInstanceExtensionProperties
            xrEnumerateInstanceExtensionProperties;
        OXR(result = xrGetInstanceProcAddr(
                XR_NULL_HANDLE, "xrEnumerateInstanceExtensionProperties",
                (PFN_xrVoidFunction*)&xrEnumerateInstanceExtensionProperties));
        if (result != XR_SUCCESS)
        {
            ALOGE("Failed to get xrEnumerateInstanceExtensionProperties "
                  "function pointer.");
            return -1;
        }

        uint32_t numInputExtensions  = 0;
        uint32_t numOutputExtensions = 0;
        OXR(xrEnumerateInstanceExtensionProperties(NULL, numInputExtensions,
                                                   &numOutputExtensions, NULL));
        ALOGV("xrEnumerateInstanceExtensionProperties found %u extension(s).",
              numOutputExtensions);

        numInputExtensions = numOutputExtensions;

        auto extensionProperties =
            std::vector<XrExtensionProperties>(numOutputExtensions);

        for (auto& ext : extensionProperties)
        {
            ext.type = XR_TYPE_EXTENSION_PROPERTIES;
            ext.next = NULL;
        }

        OXR(xrEnumerateInstanceExtensionProperties(NULL, numInputExtensions,
                                                   &numOutputExtensions,
                                                   extensionProperties.data()));
#ifndef NDEBUG
        for (uint32_t i = 0; i < numOutputExtensions; i++)
        {
            ALOGV("Extension #%d = '%s'.", i,
                  extensionProperties[i].extensionName);
        }
#endif

        for (uint32_t i = 0; i < numRequiredExtensions; i++)
        {
            bool found = false;
            for (uint32_t j = 0; j < numOutputExtensions; j++)
            {
                if (!strcmp(requiredExtensionNames[i],
                            extensionProperties[j].extensionName))
                {
                    ALOGD("Found required extension %s",
                          requiredExtensionNames[i]);
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                ALOGE("Failed to find required extension %s",
                      requiredExtensionNames[i]);
                return -2;
            }
        }
    }
    return 0;
}

XrInstance XrInstanceCreate()
{
    // Check that the extensions required are present.
    static const char* const requiredExtensionNames[] = {
        XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,
        XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME,
        XR_KHR_ANDROID_THREAD_SETTINGS_EXTENSION_NAME,
        XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME,
        XR_KHR_COMPOSITION_LAYER_EQUIRECT2_EXTENSION_NAME,
        XR_KHR_ANDROID_SURFACE_SWAPCHAIN_EXTENSION_NAME,
        XR_FB_COMPOSITION_LAYER_SETTINGS_EXTENSION_NAME,
        XR_FB_PASSTHROUGH_EXTENSION_NAME,

    };
    static constexpr size_t numRequiredExtensions =
        sizeof(requiredExtensionNames) / sizeof(requiredExtensionNames[0]);

    BAIL_ON_ERR(XrCheckRequiredExtensions(&requiredExtensionNames[0],
                                          numRequiredExtensions),
                XR_NULL_HANDLE);

    XrApplicationInfo appInfo = {};
    strcpy(appInfo.applicationName, "Citra");
    appInfo.applicationVersion = 0;
    strcpy(appInfo.engineName, "custom");
    appInfo.engineVersion = 0;
    appInfo.apiVersion    = XR_CURRENT_API_VERSION;

    XrInstanceCreateInfo ici  = {};
    ici.type                  = XR_TYPE_INSTANCE_CREATE_INFO;
    ici.next                  = nullptr;
    ici.createFlags           = 0;
    ici.applicationInfo       = appInfo;
    ici.enabledApiLayerCount  = 0;
    ici.enabledApiLayerNames  = NULL;
    ici.enabledExtensionCount = numRequiredExtensions;
    ici.enabledExtensionNames = requiredExtensionNames;

    XrResult   initResult;
    XrInstance instanceLocal;
    OXR(initResult = xrCreateInstance(&ici, &instanceLocal));
    if (initResult != XR_SUCCESS)
    {
        ALOGE("ERROR(%s()): Failed to create XR instance_: %d.", __FUNCTION__,
              initResult);
        return XR_NULL_HANDLE;
    }
    // Log runtime instance info
    {
        XrInstanceProperties instanceInfo;
        instanceInfo.type = XR_TYPE_INSTANCE_PROPERTIES;
        instanceInfo.next = NULL;
        OXR(xrGetInstanceProperties(instanceLocal, &instanceInfo));
        ALOGV("Runtime %s: Version : %u.%u.%u", instanceInfo.runtimeName,
              XR_VERSION_MAJOR(instanceInfo.runtimeVersion),
              XR_VERSION_MINOR(instanceInfo.runtimeVersion),
              XR_VERSION_PATCH(instanceInfo.runtimeVersion));
    }
    return instanceLocal;
}

// Next return code: -2
int32_t XrInitializeLoaderTrampoline(JavaVM* jvm, jobject activityObject)
{
    PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR;
    xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
                          (PFN_xrVoidFunction*)&xrInitializeLoaderKHR);
    if (xrInitializeLoaderKHR != nullptr)
    {
        XrLoaderInitInfoAndroidKHR loaderInitializeInfoAndroid;
        memset(&loaderInitializeInfoAndroid, 0,
               sizeof(loaderInitializeInfoAndroid));
        loaderInitializeInfoAndroid.type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;
        loaderInitializeInfoAndroid.next = NULL;
        loaderInitializeInfoAndroid.applicationVM      = jvm;
        loaderInitializeInfoAndroid.applicationContext = activityObject;
        xrInitializeLoaderKHR(
            (XrLoaderInitInfoBaseHeaderKHR*)&loaderInitializeInfoAndroid);
    }
    else
    {
        ALOGE("%s(): xrInitializeLoaderKHR is NULL", __FUNCTION__);
        return -1;
    }
    return 0;
}

XrSession XrSessionCreate(const XrInstance&                  localInstance,
                          const XrSystemId&                  systemId,
                          const std::unique_ptr<EglContext>& egl)
{
    XrGraphicsBindingOpenGLESAndroidKHR graphicsBinding = {};
    graphicsBinding.type    = XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR;
    graphicsBinding.next    = NULL;
    graphicsBinding.display = egl->mDisplay;
    graphicsBinding.config  = egl->mConfig;
    graphicsBinding.context = egl->mContext;

    XrSessionCreateInfo sessionCreateInfo = {};
    sessionCreateInfo.type                = XR_TYPE_SESSION_CREATE_INFO;
    sessionCreateInfo.next                = &graphicsBinding;
    sessionCreateInfo.createFlags         = 0;
    sessionCreateInfo.systemId            = systemId;

    XrSession session;
    XrResult  initResult;
    OXR(initResult =
            xrCreateSession(localInstance, &sessionCreateInfo, &session));
    if (initResult != XR_SUCCESS)
    {
        ALOGE("Failed to create XR session: %d.", initResult);
        return XR_NULL_HANDLE;
    }
    return session;
}

XrSystemId XrGetSystemId(const XrInstance& instanceLocal)
{
    XrSystemId systemId = XR_NULL_SYSTEM_ID;

    XrSystemGetInfo sgi = {};
    sgi.type            = XR_TYPE_SYSTEM_GET_INFO;
    sgi.next            = NULL;
    sgi.formFactor      = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

    XrResult initResult;
    OXR(initResult = xrGetSystem(instanceLocal, &sgi, &systemId));
    if (initResult != XR_SUCCESS)
    {
        ALOGE("ERROR (%s()): Failed to get system.", __FUNCTION__);
        return XR_NULL_SYSTEM_ID;
    }
    return systemId;
}

// TODO give users a choice, since the highest refresh rate also drains battery
// life.
void SetHighestRefreshRate(const XrSession& session)
{
    // attempts to set desired refresh rate. If not available, sets highest
    // refresh rate.
    static constexpr float desiredRefreshRate = 90.0f;
    // Enumerate refresh rates
    uint32_t numRefreshRates = 0;
    OXR(xrEnumerateDisplayRefreshRatesFB(session, 0, &numRefreshRates,
                                         nullptr));
    std::vector<float> availableRefreshRates(numRefreshRates);
    OXR(xrEnumerateDisplayRefreshRatesFB(session, availableRefreshRates.size(),
                                         &numRefreshRates,
                                         availableRefreshRates.data()));
    assert((size_t)numRefreshRates == availableRefreshRates.size());
    bool  hasDesiredRefreshRate = false;
    float maxRefreshRate        = 0;
    for (auto r : availableRefreshRates)
    {
        if (r == desiredRefreshRate) { hasDesiredRefreshRate = true; }
        if (r > maxRefreshRate) { maxRefreshRate = r; }
    }
    const float refreshRate =
        hasDesiredRefreshRate ? desiredRefreshRate : maxRefreshRate;
    ALOGI("Setting max refresh rate %.1fHz", refreshRate);
    OXR(xrRequestDisplayRefreshRateFB(session, refreshRate));
}

size_t GetMaxLayerCount(const XrInstance& instanceLocal,
                        const XrSystemId& systemId)
{
    XrSystemProperties systemProperties = {};
    systemProperties.type               = XR_TYPE_SYSTEM_PROPERTIES;
    OXR(xrGetSystemProperties(instanceLocal, systemId, &systemProperties));

    ALOGV("System Properties: Name=%s VendorId=%x", systemProperties.systemName,
          systemProperties.vendorId);
    ALOGV("System Graphics Properties: MaxWidth=%d MaxHeight=%d MaxLayers=%d",
          systemProperties.graphicsProperties.maxSwapchainImageWidth,
          systemProperties.graphicsProperties.maxSwapchainImageHeight,
          systemProperties.graphicsProperties.maxLayerCount);
    ALOGV("System Tracking Properties: OrientationTracking=%s "
          "PositionTracking=%s",
          systemProperties.trackingProperties.orientationTracking ? "True"
                                                                  : "False",
          systemProperties.trackingProperties.positionTracking ? "True"
                                                               : "False");

    return systemProperties.graphicsProperties.maxLayerCount;
}

} // anonymous namespace

XrInstance& OpenXr::GetInstance() { return instance; }

int32_t OpenXr::Init(JavaVM* const jvm, const jobject activityObject)
{
    for (int eye = 0; eye < 2; eye++) { viewConfigurationViews_[eye] = {}; }
    BAIL_ON_ERR(OpenXRInit(jvm, activityObject), -1);
    BAIL_ON_ERR(XrViewConfigInit(), -2);
    BAIL_ON_ERR(XrSpaceInit(), -3);

    return 0;
}

// Next return code: -2
int32_t OpenXr::XrViewConfigInit()
{
    // Enumerate the viewport configurations.
    uint32_t viewportConfigTypeCount = 0;
    OXR(xrEnumerateViewConfigurations(instance_, systemId_, 0,
                                      &viewportConfigTypeCount, NULL));

    auto viewportConfigurationTypes =
        std::vector<XrViewConfigurationType>(viewportConfigTypeCount);

    OXR(xrEnumerateViewConfigurations(
        instance_, systemId_, viewportConfigTypeCount, &viewportConfigTypeCount,
        viewportConfigurationTypes.data()));

    ALOGV("Available Viewport Configuration Types: %d",
          viewportConfigTypeCount);

    bool foundSupportedViewport;
    for (uint32_t i = 0; i < viewportConfigTypeCount; i++)
    {
        const XrViewConfigurationType viewportConfigType =
            viewportConfigurationTypes[i];

        ALOGV("Viewport configuration type %d : %s", viewportConfigType,
              viewportConfigType == VIEW_CONFIG_TYPE ? "Selected" : "");

        XrViewConfigurationProperties viewportConfig;
        viewportConfig.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES;
        OXR(xrGetViewConfigurationProperties(
            instance_, systemId_, viewportConfigType, &viewportConfig));
        ALOGV("FovMutable=%s ConfigurationType %d",
              viewportConfig.fovMutable ? "true" : "false",
              viewportConfig.viewConfigurationType);

        uint32_t viewCount;
        OXR(xrEnumerateViewConfigurationViews(
            instance_, systemId_, viewportConfigType, 0, &viewCount, NULL));

        if (viewCount > 0)
        {
            auto elements = std::vector<XrViewConfigurationView>(viewCount);

            for (uint32_t e = 0; e < viewCount; e++)
            {
                elements[e].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
                elements[e].next = NULL;
            }

            OXR(xrEnumerateViewConfigurationViews(instance_, systemId_,
                                                  viewportConfigType, viewCount,
                                                  &viewCount, elements.data()));

            // Log the view config info for each view type for debugging
            // purposes.
            for (uint32_t e = 0; e < viewCount; e++)
            {
                const XrViewConfigurationView* element = &elements[e];
                (void)element;

                ALOGV("Viewport [%d]: Recommended Width=%d Height=%d "
                      "SampleCount=%d",
                      e, element->recommendedImageRectWidth,
                      element->recommendedImageRectHeight,
                      element->recommendedSwapchainSampleCount);

                ALOGV("Viewport [%d]: Max Width=%d Height=%d SampleCount=%d", e,
                      element->maxImageRectWidth, element->maxImageRectHeight,
                      element->maxSwapchainSampleCount);
            }

            // Cache the view config properties for the selected config type.
            if (viewportConfigType == VIEW_CONFIG_TYPE)
            {
                foundSupportedViewport = true;
                assert(viewCount == NUM_EYES);
                for (uint32_t e = 0; e < viewCount; e++)
                {
                    viewConfigurationViews_[e] = elements[e];
                }
            }
        }
        else { ALOGD("Empty viewport configuration type: %d", viewCount); }
    }
    if (!foundSupportedViewport)
    {
        ALOGE("No supported viewport found");
        return -1;
    }

    // Get the viewport configuration info for the chosen viewport configuration
    // type.
    viewportConfig_.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES;

    OXR(xrGetViewConfigurationProperties(instance_, systemId_, VIEW_CONFIG_TYPE,
                                         &viewportConfig_));
    return 0;
}

int32_t OpenXr::XrSpaceInit()
{
    bool stageSupported = false;

    uint32_t numOutputSpaces = 0;
    OXR(xrEnumerateReferenceSpaces(session_, 0, &numOutputSpaces, NULL));

    auto referenceSpaces = std::vector<XrReferenceSpaceType>(numOutputSpaces);

    OXR(xrEnumerateReferenceSpaces(session_, numOutputSpaces, &numOutputSpaces,
                                   referenceSpaces.data()));

    for (uint32_t i = 0; i < numOutputSpaces; i++)
    {
        if (referenceSpaces[i] == XR_REFERENCE_SPACE_TYPE_STAGE)
        {
            stageSupported = true;
            break;
        }
    }

    // Create a space to the first path
    {
        XrReferenceSpaceCreateInfo spaceCreateInfo = {};
        spaceCreateInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
        spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
        spaceCreateInfo.poseInReferenceSpace.orientation.w = 1.0f;
        OXR(xrCreateReferenceSpace(session_, &spaceCreateInfo, &headSpace_));

        spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        OXR(xrCreateReferenceSpace(session_, &spaceCreateInfo, &localSpace_));
    }

    if (stageSupported)
    {
        XrReferenceSpaceCreateInfo spaceCreateInfo = {
            XR_TYPE_REFERENCE_SPACE_CREATE_INFO};

        spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
        spaceCreateInfo.poseInReferenceSpace.orientation.w = 1.0f;
        OXR(xrCreateReferenceSpace(session_, &spaceCreateInfo, &stageSpace_));
    }

    return 0;
}

void OpenXr::XrSpaceDestroy()
{
    if (headSpace_ != XR_NULL_HANDLE)
    {
        OXR(xrDestroySpace(headSpace_));
        headSpace_ = XR_NULL_HANDLE;
    }
    if (localSpace_ != XR_NULL_HANDLE)
    {
        OXR(xrDestroySpace(localSpace_));
        localSpace_ = XR_NULL_HANDLE;
    }
    if (stageSpace_ != XR_NULL_HANDLE)
    {
        OXR(xrDestroySpace(stageSpace_));
        stageSpace_ = XR_NULL_HANDLE;
    }
}

// Next return code: -7
int OpenXr::OpenXRInit(JavaVM* const jvm, const jobject activityObject)
{

    /////////////////////////////////////
    // Initialize OpenXR loader
    /////////////////////////////////////
    BAIL_ON_ERR(XrInitializeLoaderTrampoline(jvm, activityObject), -1);

    /////////////////////////////////////
    // Create the OpenXR instance.
    /////////////////////////////////////
    instance_ = XrInstanceCreate();
    if (instance_ == XR_NULL_HANDLE)
    {
        ALOGE("Failed to create XR instance");
        return -2;
    }
    // Set the global used in macros
    instance = instance_;

    systemId_ = XrGetSystemId(instance_);
    if (systemId_ == XR_NULL_SYSTEM_ID)
    {
        ALOGE("Failed to retrieve XR system ID");
        return -3;
    }

    maxLayerCount_ = GetMaxLayerCount(instance_, systemId_);

    ////////////////////////////////
    // Init EGL
    ////////////////////////////////
    {
        // Get the graphics requirements.
        PFN_xrGetOpenGLESGraphicsRequirementsKHR
            pfnGetOpenGLESGraphicsRequirementsKHR = NULL;
        OXR(xrGetInstanceProcAddr(
            instance_, "xrGetOpenGLESGraphicsRequirementsKHR",
            (PFN_xrVoidFunction*)(&pfnGetOpenGLESGraphicsRequirementsKHR)));

        XrGraphicsRequirementsOpenGLESKHR graphicsRequirements = {};
        graphicsRequirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR;
        OXR(pfnGetOpenGLESGraphicsRequirementsKHR(instance_, systemId_,
                                                  &graphicsRequirements));

        {
            // Create the EGL Context
            eglContext_ = std::make_unique<EglContext>();

            // Check the graphics requirements.
            int32_t eglMajor = 0;
            int32_t eglMinor = 0;
            glGetIntegerv(GL_MAJOR_VERSION, &eglMajor);
            glGetIntegerv(GL_MINOR_VERSION, &eglMinor);
            const XrVersion eglVersion = XR_MAKE_VERSION(eglMajor, eglMinor, 0);
            if (eglVersion < graphicsRequirements.minApiVersionSupported ||
                eglVersion > graphicsRequirements.maxApiVersionSupported)
            {
                ALOGE("GLES version %d.%d not supported", eglMajor, eglMinor);
                return -5;
            }
        }
    }

    ///////////////////////////////////////////
    // Initialize the global OpenXR functions
    ///////////////////////////////////////////
    InitOXRFunctions(instance_);

    ///////////////////////////////
    // Create the OpenXR Session.
    //////////////////////////////
    session_ = XrSessionCreate(instance, systemId_, eglContext_);
    if (session_ == XR_NULL_HANDLE)
    {
        ALOGE("Failed to create XR session");
        return -6;
    }

    SetHighestRefreshRate(session_);

    return 0;
}

void OpenXr::Shutdown()
{
    XrSpaceDestroy();

    if (session_ != XR_NULL_HANDLE)
    {
        OXR(xrDestroySession(session_));
        session_ = XR_NULL_HANDLE;
    }

    if (instance_ != XR_NULL_HANDLE)
    {
        OXR(xrDestroyInstance(instance_));
        instance_ = XR_NULL_HANDLE;
    }
}
