/*******************************************************************************

Filename    :   OpenXR.cpp

Content     :   OpenXR initialization and shutdown code.

Authors     :   Amanda M. Watson
License     :   Licensed under GPLv3 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#include "OpenXR.h"

#include "utils/Common.h"
#include "utils/LogUtils.h"

#include <openxr/openxr_platform.h>

#include <vector>

#include <assert.h>

#define BAIL_ON_ERR(fn, returnCode)                                                                \
    do {                                                                                           \
        const int32_t ret = fn;                                                                    \
        if (ret < 0) {                                                                             \
            ALOGE("ERROR ({}): {}() returned {}", __FUNCTION__, #fn, ret);                         \
            return (returnCode);                                                                   \
        }                                                                                          \
    } while (0)

XrInstance instance;
void OXR_CheckErrors(XrResult result, const char* function, bool failOnError) {
    if (XR_FAILED(result)) {
        char errorBuffer[XR_MAX_RESULT_STRING_SIZE];
        xrResultToString(instance, result, errorBuffer);
        if (failOnError) {
            FAIL("OpenXR error: %s: %s\n", function, errorBuffer);
        } else {
            ALOGV("OpenXR error: {}: {}\n", function, errorBuffer);
        }
    }
}

/*
   ================================================================================

   OpenXR Utility Functions

   ================================================================================
   */
namespace {
#define DECL_PFN(pfn) PFN_##pfn pfn = nullptr
#define INIT_PFN(pfn) OXR(xrGetInstanceProcAddr(instance, #pfn, (PFN_xrVoidFunction*)(&pfn)))

[[maybe_unused]] void XrEnumerateLayerProperties() {
    XrResult result;
    PFN_xrEnumerateApiLayerProperties xrEnumerateApiLayerProperties;
    OXR(result = xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrEnumerateApiLayerProperties",
                                       (PFN_xrVoidFunction*)&xrEnumerateApiLayerProperties));
    if (result != XR_SUCCESS) {
        FAIL("Failed to get xrEnumerateApiLayerProperties function pointer.");
    }

    uint32_t numInputLayers = 0;
    uint32_t numOutputLayers = 0;
    OXR(xrEnumerateApiLayerProperties(numInputLayers, &numOutputLayers, NULL));

    numInputLayers = numOutputLayers;

    auto layerProperties = std::vector<XrApiLayerProperties>(numOutputLayers);

    for (auto& lp : layerProperties) {
        lp.type = XR_TYPE_API_LAYER_PROPERTIES;
        lp.next = NULL;
    }

    OXR(xrEnumerateApiLayerProperties(numInputLayers, &numOutputLayers, layerProperties.data()));

    for (uint32_t i = 0; i < numOutputLayers; i++) {
        ALOGI("Found layer {}", layerProperties[i].layerName);
    }
}

// Next return code: -3
int XrCheckRequiredExtensions(const char* const* requiredExtensionNames,
                              const size_t numRequiredExtensions) {

#ifndef NDEBUG
    XrEnumerateLayerProperties();
#endif

    // Check the list of required extensions against what is supported by the
    // runtime.
    {
        XrResult result;
        PFN_xrEnumerateInstanceExtensionProperties xrEnumerateInstanceExtensionProperties;
        OXR(result = xrGetInstanceProcAddr(
                XR_NULL_HANDLE, "xrEnumerateInstanceExtensionProperties",
                (PFN_xrVoidFunction*)&xrEnumerateInstanceExtensionProperties));
        if (result != XR_SUCCESS) {
            ALOGE("Failed to get xrEnumerateInstanceExtensionProperties "
                  "function pointer.");
            return -1;
        }

        uint32_t numInputExtensions = 0;
        uint32_t numOutputExtensions = 0;
        OXR(xrEnumerateInstanceExtensionProperties(NULL, numInputExtensions, &numOutputExtensions,
                                                   NULL));
        ALOGV("xrEnumerateInstanceExtensionProperties found {} extension(s).", numOutputExtensions);

        numInputExtensions = numOutputExtensions;

        auto extensionProperties = std::vector<XrExtensionProperties>(numOutputExtensions);

        for (auto& ext : extensionProperties) {
            ext.type = XR_TYPE_EXTENSION_PROPERTIES;
            ext.next = NULL;
        }

        OXR(xrEnumerateInstanceExtensionProperties(NULL, numInputExtensions, &numOutputExtensions,
                                                   extensionProperties.data()));
#ifndef NDEBUG
        for (uint32_t i = 0; i < numOutputExtensions; i++) {
            ALOGV("Extension #{} = '{}'.", i, extensionProperties[i].extensionName);
        }
#endif

        for (uint32_t i = 0; i < numRequiredExtensions; i++) {
            bool found = false;
            for (uint32_t j = 0; j < numOutputExtensions; j++) {
                if (!strcmp(requiredExtensionNames[i], extensionProperties[j].extensionName)) {
                    ALOGD("Found required extension {}", requiredExtensionNames[i]);
                    found = true;
                    break;
                }
            }
            if (!found) {
                ALOGE("Failed to find required extension {}", requiredExtensionNames[i]);
                return -2;
            }
        }
    }
    return 0;
}

XrInstance XrInstanceCreate() {
    // Check that the extensions required are present.
    static const char* const requiredExtensionNames[] = {
        XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,
        XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME,
        XR_KHR_ANDROID_THREAD_SETTINGS_EXTENSION_NAME,
        XR_KHR_COMPOSITION_LAYER_EQUIRECT2_EXTENSION_NAME,
        XR_KHR_ANDROID_SURFACE_SWAPCHAIN_EXTENSION_NAME,
        XR_FB_COMPOSITION_LAYER_SETTINGS_EXTENSION_NAME,
        XR_FB_PASSTHROUGH_EXTENSION_NAME,

    };
    static constexpr size_t numRequiredExtensions =
        sizeof(requiredExtensionNames) / sizeof(requiredExtensionNames[0]);

    BAIL_ON_ERR(XrCheckRequiredExtensions(&requiredExtensionNames[0], numRequiredExtensions),
                XR_NULL_HANDLE);

    XrApplicationInfo appInfo = {};
    strcpy(appInfo.applicationName, "Citra");
    appInfo.applicationVersion = 0;
    strcpy(appInfo.engineName, "custom");
    appInfo.engineVersion = 0;
    appInfo.apiVersion = XR_CURRENT_API_VERSION;

    XrInstanceCreateInfo ici = {};
    ici.type = XR_TYPE_INSTANCE_CREATE_INFO;
    ici.next = nullptr;
    ici.createFlags = 0;
    ici.applicationInfo = appInfo;
    ici.enabledApiLayerCount = 0;
    ici.enabledApiLayerNames = NULL;
    ici.enabledExtensionCount = numRequiredExtensions;
    ici.enabledExtensionNames = requiredExtensionNames;

    XrResult initResult;
    XrInstance instanceLocal;
    OXR(initResult = xrCreateInstance(&ici, &instanceLocal));
    if (initResult != XR_SUCCESS) {
        ALOGE("ERROR({}()): Failed to create XR mInstance: {}.", __FUNCTION__, initResult);
        return XR_NULL_HANDLE;
    }
    // Log runtime instance info
    {
        XrInstanceProperties instanceInfo;
        instanceInfo.type = XR_TYPE_INSTANCE_PROPERTIES;
        instanceInfo.next = NULL;
        OXR(xrGetInstanceProperties(instanceLocal, &instanceInfo));
        ALOGV("Runtime {}: Version : {}.{}.{}", instanceInfo.runtimeName,
              XR_VERSION_MAJOR(instanceInfo.runtimeVersion),
              XR_VERSION_MINOR(instanceInfo.runtimeVersion),
              XR_VERSION_PATCH(instanceInfo.runtimeVersion));
    }
    return instanceLocal;
}

// Next return code: -2
int32_t XrInitializeLoaderTrampoline(JavaVM* jvm, jobject activityObject) {
    PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR;
    xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
                          (PFN_xrVoidFunction*)&xrInitializeLoaderKHR);
    if (xrInitializeLoaderKHR != nullptr) {
        XrLoaderInitInfoAndroidKHR loaderInitializeInfoAndroid;
        memset(&loaderInitializeInfoAndroid, 0, sizeof(loaderInitializeInfoAndroid));
        loaderInitializeInfoAndroid.type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;
        loaderInitializeInfoAndroid.next = NULL;
        loaderInitializeInfoAndroid.applicationVM = jvm;
        loaderInitializeInfoAndroid.applicationContext = activityObject;
        xrInitializeLoaderKHR((XrLoaderInitInfoBaseHeaderKHR*)&loaderInitializeInfoAndroid);
    } else {
        ALOGE("{}(): xrInitializeLoaderKHR is NULL", __FUNCTION__);
        return -1;
    }
    return 0;
}

XrSession XrSessionCreate(const XrInstance& localInstance, const XrSystemId& systemId,
                          const std::unique_ptr<EglContext>& egl) {
    XrGraphicsBindingOpenGLESAndroidKHR graphicsBinding = {};
    graphicsBinding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR;
    graphicsBinding.next = NULL;
    graphicsBinding.display = egl->mDisplay;
    graphicsBinding.config = egl->mConfig;
    graphicsBinding.context = egl->mContext;

    XrSessionCreateInfo sessionCreateInfo = {};
    sessionCreateInfo.type = XR_TYPE_SESSION_CREATE_INFO;
    sessionCreateInfo.next = &graphicsBinding;
    sessionCreateInfo.createFlags = 0;
    sessionCreateInfo.systemId = systemId;

    XrSession session;
    XrResult initResult;
    OXR(initResult = xrCreateSession(localInstance, &sessionCreateInfo, &session));
    if (initResult != XR_SUCCESS) {
        ALOGE("Failed to create XR session: {}.", initResult);
        return XR_NULL_HANDLE;
    }
    return session;
}

XrSystemId XrGetSystemId(const XrInstance& instanceLocal) {
    XrSystemId systemId = XR_NULL_SYSTEM_ID;

    XrSystemGetInfo sgi = {};
    sgi.type = XR_TYPE_SYSTEM_GET_INFO;
    sgi.next = NULL;
    sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

    XrResult initResult;
    OXR(initResult = xrGetSystem(instanceLocal, &sgi, &systemId));
    if (initResult != XR_SUCCESS) {
        ALOGE("ERROR ({}()): Failed to get system.", __FUNCTION__);
        return XR_NULL_SYSTEM_ID;
    }
    return systemId;
}

size_t GetMaxLayerCount(const XrInstance& instanceLocal, const XrSystemId& systemId) {
    XrSystemProperties systemProperties = {};
    systemProperties.type = XR_TYPE_SYSTEM_PROPERTIES;
    OXR(xrGetSystemProperties(instanceLocal, systemId, &systemProperties));

    ALOGV("System Properties: Name={} VendorId={}", systemProperties.systemName,
          systemProperties.vendorId);
    ALOGV("System Graphics Properties: MaxWidth={} MaxHeight={} MaxLayers={}",
          systemProperties.graphicsProperties.maxSwapchainImageWidth,
          systemProperties.graphicsProperties.maxSwapchainImageHeight,
          systemProperties.graphicsProperties.maxLayerCount);
    ALOGV("System Tracking Properties: OrientationTracking={} "
          "PositionTracking={}",
          systemProperties.trackingProperties.orientationTracking ? "True" : "False",
          systemProperties.trackingProperties.positionTracking ? "True" : "False");

    return systemProperties.graphicsProperties.maxLayerCount;
}
} // anonymous namespace

XrInstance& OpenXr::GetInstance() {
    return instance;
}

int32_t OpenXr::Init(JavaVM* const jvm, const jobject activityObject) {
    for (int eye = 0; eye < 2; eye++) {
        mViewConfigurationViews[eye] = {};
    }
    BAIL_ON_ERR(OpenXRInit(jvm, activityObject), -1);
    BAIL_ON_ERR(XrViewConfigInit(), -2);
    BAIL_ON_ERR(XrSpaceInit(), -3);

    return 0;
}

// Next return code: -2
int32_t OpenXr::XrViewConfigInit() {
    // Enumerate the viewport configurations.
    uint32_t viewportConfigTypeCount = 0;
    OXR(xrEnumerateViewConfigurations(mInstance, mSystemId, 0, &viewportConfigTypeCount, NULL));

    auto viewportConfigurationTypes = std::vector<XrViewConfigurationType>(viewportConfigTypeCount);

    OXR(xrEnumerateViewConfigurations(mInstance, mSystemId, viewportConfigTypeCount,
                                      &viewportConfigTypeCount, viewportConfigurationTypes.data()));

    ALOGV("Available Viewport Configuration Types: {}", viewportConfigTypeCount);

    bool foundSupportedViewport;
    for (uint32_t i = 0; i < viewportConfigTypeCount; i++) {
        const XrViewConfigurationType viewportConfigType = viewportConfigurationTypes[i];

        ALOGV("Viewport configuration type {} : {}", viewportConfigType,
              viewportConfigType == VIEW_CONFIG_TYPE ? "Selected" : "");

        XrViewConfigurationProperties viewportConfig;
        viewportConfig.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES;
        OXR(xrGetViewConfigurationProperties(mInstance, mSystemId, viewportConfigType,
                                             &viewportConfig));
        ALOGV("FovMutable={} ConfigurationType {}", viewportConfig.fovMutable ? "true" : "false",
              viewportConfig.viewConfigurationType);

        uint32_t viewCount;
        OXR(xrEnumerateViewConfigurationViews(mInstance, mSystemId, viewportConfigType, 0,
                                              &viewCount, NULL));

        if (viewCount > 0) {
            auto elements = std::vector<XrViewConfigurationView>(viewCount);

            for (uint32_t e = 0; e < viewCount; e++) {
                elements[e].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
                elements[e].next = NULL;
            }

            OXR(xrEnumerateViewConfigurationViews(mInstance, mSystemId, viewportConfigType,
                                                  viewCount, &viewCount, elements.data()));

            // Log the view config info for each view type for debugging
            // purposes.
            for (uint32_t e = 0; e < viewCount; e++) {
                const XrViewConfigurationView* element = &elements[e];
                (void)element;

                ALOGV("Viewport [{}]: Recommended Width={} Height={} "
                      "SampleCount={}",
                      e, element->recommendedImageRectWidth, element->recommendedImageRectHeight,
                      element->recommendedSwapchainSampleCount);

                ALOGV("Viewport [{}]: Max Width={} Height={} SampleCount={}", e,
                      element->maxImageRectWidth, element->maxImageRectHeight,
                      element->maxSwapchainSampleCount);
            }

            // Cache the view config properties for the selected config type.
            if (viewportConfigType == VIEW_CONFIG_TYPE) {
                foundSupportedViewport = true;
                assert(viewCount == NUM_EYES);
                for (uint32_t e = 0; e < viewCount; e++) {
                    mViewConfigurationViews[e] = elements[e];
                }
            }
        } else {
            ALOGD("Empty viewport configuration type: {}", viewCount);
        }
    }
    if (!foundSupportedViewport) {
        ALOGE("No supported viewport found");
        return -1;
    }

    // Get the viewport configuration info for the chosen viewport configuration
    // type.
    mViewportConfig.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES;

    OXR(xrGetViewConfigurationProperties(mInstance, mSystemId, VIEW_CONFIG_TYPE, &mViewportConfig));
    return 0;
}

int32_t OpenXr::XrSpaceInit() {
    bool stageSupported = false;

    uint32_t numOutputSpaces = 0;
    OXR(xrEnumerateReferenceSpaces(mSession, 0, &numOutputSpaces, NULL));

    auto referenceSpaces = std::vector<XrReferenceSpaceType>(numOutputSpaces);

    OXR(xrEnumerateReferenceSpaces(mSession, numOutputSpaces, &numOutputSpaces,
                                   referenceSpaces.data()));

    for (uint32_t i = 0; i < numOutputSpaces; i++) {
        if (referenceSpaces[i] == XR_REFERENCE_SPACE_TYPE_STAGE) {
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
        OXR(xrCreateReferenceSpace(mSession, &spaceCreateInfo, &mHeadSpace));

        spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        OXR(xrCreateReferenceSpace(mSession, &spaceCreateInfo, &mLocalSpace));
    }

    if (stageSupported) {
        XrReferenceSpaceCreateInfo spaceCreateInfo = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};

        spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
        spaceCreateInfo.poseInReferenceSpace.orientation.w = 1.0f;
        OXR(xrCreateReferenceSpace(mSession, &spaceCreateInfo, &mStageSpace));
    }

    return 0;
}

void OpenXr::XrSpaceDestroy() {
    if (mHeadSpace != XR_NULL_HANDLE) {
        OXR(xrDestroySpace(mHeadSpace));
        mHeadSpace = XR_NULL_HANDLE;
    }
    if (mLocalSpace != XR_NULL_HANDLE) {
        OXR(xrDestroySpace(mLocalSpace));
        mLocalSpace = XR_NULL_HANDLE;
    }
    if (mStageSpace != XR_NULL_HANDLE) {
        OXR(xrDestroySpace(mStageSpace));
        mStageSpace = XR_NULL_HANDLE;
    }
}

// Next return code: -7
int OpenXr::OpenXRInit(JavaVM* const jvm, const jobject activityObject) {

    /////////////////////////////////////
    // Initialize OpenXR loader
    /////////////////////////////////////
    BAIL_ON_ERR(XrInitializeLoaderTrampoline(jvm, activityObject), -1);

    /////////////////////////////////////
    // Create the OpenXR instance.
    /////////////////////////////////////
    mInstance = XrInstanceCreate();
    if (mInstance == XR_NULL_HANDLE) {
        ALOGE("Failed to create XR instance");
        return -2;
    }
    // Set the global used in macros
    instance = mInstance;

    mSystemId = XrGetSystemId(mInstance);
    if (mSystemId == XR_NULL_SYSTEM_ID) {
        ALOGE("Failed to retrieve XR system ID");
        return -3;
    }

    mMaxLayerCount = GetMaxLayerCount(mInstance, mSystemId);

    ////////////////////////////////
    // Init EGL
    ////////////////////////////////
    {
        // Get the graphics requirements.
        PFN_xrGetOpenGLESGraphicsRequirementsKHR pfnGetOpenGLESGraphicsRequirementsKHR = NULL;
        OXR(xrGetInstanceProcAddr(mInstance, "xrGetOpenGLESGraphicsRequirementsKHR",
                                  (PFN_xrVoidFunction*)(&pfnGetOpenGLESGraphicsRequirementsKHR)));

        XrGraphicsRequirementsOpenGLESKHR graphicsRequirements = {};
        graphicsRequirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR;
        OXR(pfnGetOpenGLESGraphicsRequirementsKHR(mInstance, mSystemId, &graphicsRequirements));

        {
            // Create the EGL Context
            mEglContext = std::make_unique<EglContext>();

            // Check the graphics requirements.
            int32_t eglMajor = 0;
            int32_t eglMinor = 0;
            glGetIntegerv(GL_MAJOR_VERSION, &eglMajor);
            glGetIntegerv(GL_MINOR_VERSION, &eglMinor);
            const XrVersion eglVersion = XR_MAKE_VERSION(eglMajor, eglMinor, 0);
            if (eglVersion < graphicsRequirements.minApiVersionSupported ||
                eglVersion > graphicsRequirements.maxApiVersionSupported) {
                ALOGE("GLES version {}.{} not supported", eglMajor, eglMinor);
                return -5;
            }
        }
    }

    ///////////////////////////////
    // Create the OpenXR Session.
    //////////////////////////////
    mSession = XrSessionCreate(instance, mSystemId, mEglContext);
    if (mSession == XR_NULL_HANDLE) {
        ALOGE("Failed to create XR session");
        return -6;
    }
    return 0;
}

void OpenXr::Shutdown() {
    XrSpaceDestroy();

    if (mSession != XR_NULL_HANDLE) {
        OXR(xrDestroySession(mSession));
        mSession = XR_NULL_HANDLE;
    }

    if (mInstance != XR_NULL_HANDLE) {
        OXR(xrDestroyInstance(mInstance));
        mInstance = XR_NULL_HANDLE;
    }
}
