/*******************************************************************************

Filename    :   CursorLayer.cpp
Content     :   Renders a cursor
Authors     :   Amanda M. Watson
License     :   Licensed under GPLv2 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#include "CursorLayer.h"

#include "../OpenXR.h"
#include "../utils/LogUtils.h"
#include "../utils/XrMath.h"

namespace {

constexpr uint32_t CURSOR_WIDTH = 16;
constexpr uint32_t CURSOR_HEIGHT = CURSOR_WIDTH;
constexpr uint32_t SUPER_SAMPLE_FACTOR = 2;

constexpr uint8_t CURSOR_ALPHA = 255;

constexpr uint8_t COLOR_WHITE_RGB[3] = {255, 255, 255}; // White color for the cursor.
constexpr uint8_t COLOR_CYAN_RGB[3] = {10, 185, 180};   // Cyan color for the cursor.

constexpr uint8_t OUTLINE_COLOR_RGB[3] = {0, 0, 0}; // Black color for the outline.
constexpr uint32_t OUTLINE_THICKNESS = 2;

typedef std::array<uint8_t, CURSOR_WIDTH * CURSOR_HEIGHT * 4> CursorBuffer;

// Note: this programming style assumes C++17 (guaranteed copy elision).
// Just warn as this is one-time init code -- efficiency isn't necessary --
// but I like to keep track of inefficiencies.
#if __cplusplus < 201703L
#warning "CursorLayer.cpp is designed to be compiled with C++17 or later."
#endif

Swapchain CreateSwapchain(const XrSession& session) {
    Swapchain swapchain;
    XrSwapchainCreateInfo swapChainCreateInfo;
    memset(&swapChainCreateInfo, 0, sizeof(swapChainCreateInfo));
    swapChainCreateInfo.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
    swapChainCreateInfo.createFlags = XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT;
    swapChainCreateInfo.usageFlags =
        XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    swapChainCreateInfo.format = GL_SRGB8_ALPHA8;
    swapChainCreateInfo.sampleCount = 1;
    swapChainCreateInfo.width = CURSOR_WIDTH;
    swapChainCreateInfo.height = CURSOR_HEIGHT;
    swapChainCreateInfo.faceCount = 1;
    swapChainCreateInfo.arraySize = 1;
    swapChainCreateInfo.mipCount = 1;

    swapchain.Width = swapChainCreateInfo.width;
    swapchain.Height = swapChainCreateInfo.height;

    // Create the swapchain.
    OXR(xrCreateSwapchain(session, &swapChainCreateInfo, &swapchain.Handle));
    return swapchain;
}

std::vector<XrSwapchainImageOpenGLESKHR> CreateSwapchainImages(const XrSession& session,
                                                               const XrSwapchain& xrSwapchain) {
    uint32_t length;
    OXR(xrEnumerateSwapchainImages(xrSwapchain, 0, &length, NULL));

    std::vector<XrSwapchainImageOpenGLESKHR> images =
        std::vector<XrSwapchainImageOpenGLESKHR>(length);
    for (size_t i = 0; i < images.size(); i++) {
        images[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
        images[i].next = NULL;
    }
    OXR(xrEnumerateSwapchainImages(xrSwapchain, length, &length,
                                   (XrSwapchainImageBaseHeader*)images.data()));

    return images;
}

CursorBuffer CreateSuperSampledCursorBuffer(const uint8_t colorRGB[3]) {
    constexpr uint32_t SUPER_WIDTH = CURSOR_WIDTH * SUPER_SAMPLE_FACTOR;
    constexpr uint32_t SUPER_HEIGHT = CURSOR_HEIGHT * SUPER_SAMPLE_FACTOR;
    uint8_t superCursorData[SUPER_WIDTH * SUPER_HEIGHT * 4];

    // Create the high-res cursor.
    {
        constexpr uint32_t superCenterX = SUPER_WIDTH / 2;
        constexpr uint32_t superCenterY = SUPER_HEIGHT / 2;
        constexpr uint32_t superRadius = SUPER_WIDTH / 2; // Adjust the radius as needed.
        constexpr uint32_t superOutlineThickness = OUTLINE_THICKNESS * SUPER_SAMPLE_FACTOR;

        for (uint32_t y = 0; y < SUPER_HEIGHT; y++) {
            for (uint32_t x = 0; x < SUPER_WIDTH; x++) {
                const uint32_t index = (y * SUPER_WIDTH + x) * 4;
                const int32_t dx = x - superCenterX;
                const int32_t dy = y - superCenterY;
                const uint32_t distanceSquared = dx * dx + dy * dy;

                if (distanceSquared < (superRadius * superRadius) &&
                    distanceSquared >= ((superRadius - superOutlineThickness) *
                                        (superRadius - superOutlineThickness))) {
                    // Pixel is part of the outline.
                    memcpy(&superCursorData[index], OUTLINE_COLOR_RGB, 3);
                    superCursorData[index + 3] = CURSOR_ALPHA;
                } else if (distanceSquared < ((superRadius - superOutlineThickness) *
                                              (superRadius - superOutlineThickness))) {
                    // Pixel is inside the circle.
                    memcpy(&superCursorData[index], colorRGB, 3);
                    superCursorData[index + 3] = CURSOR_ALPHA;
                } else {
                    // Pixel is outside the circle (transparent).
                    memset(&superCursorData[index], 0, 4);
                }
            }
        }
    }

    CursorBuffer cursorData;
    // Downsample to the final image.
    for (uint32_t y = 0; y < CURSOR_HEIGHT; y++) {
        for (uint32_t x = 0; x < CURSOR_WIDTH; x++) {
            uint32_t index = (y * CURSOR_WIDTH + x) * 4;
            uint32_t r = 0, g = 0, b = 0, a = 0;

            for (uint32_t dy = 0; dy < SUPER_SAMPLE_FACTOR; dy++) {
                for (uint32_t dx = 0; dx < SUPER_SAMPLE_FACTOR; dx++) {
                    const uint32_t superIndex = ((y * SUPER_SAMPLE_FACTOR + dy) * SUPER_WIDTH +
                                                 (x * SUPER_SAMPLE_FACTOR + dx)) *
                                                4;
                    r += superCursorData[superIndex];
                    g += superCursorData[superIndex + 1];
                    b += superCursorData[superIndex + 2];
                    a += superCursorData[superIndex + 3];
                }
            }

            cursorData[index] = r / (SUPER_SAMPLE_FACTOR * SUPER_SAMPLE_FACTOR);
            cursorData[index + 1] = g / (SUPER_SAMPLE_FACTOR * SUPER_SAMPLE_FACTOR);
            cursorData[index + 2] = b / (SUPER_SAMPLE_FACTOR * SUPER_SAMPLE_FACTOR);
            cursorData[index + 3] = a / (SUPER_SAMPLE_FACTOR * SUPER_SAMPLE_FACTOR);
        }
    }
    return cursorData;
}

void GenerateCursorImage(const XrSwapchain& xrSwapchain,
                         const std::vector<XrSwapchainImageOpenGLESKHR>& xrImages,
                         const uint8_t colorRGB[3]) {
    const CursorBuffer cursorData = CreateSuperSampledCursorBuffer(colorRGB);

    uint32_t index = 0;
    XrSwapchainImageAcquireInfo acquireInfo = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO, NULL};
    OXR(xrAcquireSwapchainImage(xrSwapchain, &acquireInfo, &index));

    XrSwapchainImageWaitInfo waitInfo = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO, NULL, 0};
    OXR(xrWaitSwapchainImage(xrSwapchain, &waitInfo));

    glBindTexture(GL_TEXTURE_2D, xrImages[0].image);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, CURSOR_WIDTH, CURSOR_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE,
                    cursorData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);

    XrSwapchainImageReleaseInfo releaseInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, NULL};
    OXR(xrReleaseSwapchainImage(xrSwapchain, &releaseInfo));
}

} // anonymous namespace

CursorLayer::CursorImage::CursorImage(const XrSession& session, const uint8_t colorRGB[3]) {
    mSwapchain = CreateSwapchain(session);
    mSwapchainImages = CreateSwapchainImages(session, mSwapchain.Handle);

    GenerateCursorImage(mSwapchain.Handle, mSwapchainImages, colorRGB);
}

CursorLayer::CursorImage::~CursorImage() {
    if (mSwapchain.Handle != XR_NULL_HANDLE) {
        xrDestroySwapchain(mSwapchain.Handle);
    }
    // destroy images.
    for (size_t i = 0; i < mSwapchainImages.size(); i++) {
        glDeleteTextures(1, &mSwapchainImages[i].image);
    }
}

CursorLayer::CursorLayer(const XrSession& session)
    : mCursorImages({CursorImage(session, COLOR_WHITE_RGB), CursorImage(session, COLOR_CYAN_RGB)}) {
}

void CursorLayer::Frame(const XrSpace& space, XrCompositionLayerQuad& layer,
                        const XrPosef& cursorPose, const float scaleFactor,
                        const CursorType& cursorType) const {
    layer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
    layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    layer.layerFlags |= XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
    layer.layerFlags |= XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
    layer.space = space;

    layer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
    memset(&layer.subImage, 0, sizeof(XrSwapchainSubImage));

    const auto& swapchain = mCursorImages.at(static_cast<size_t>(cursorType)).GetSwapchain();
    layer.subImage.swapchain = swapchain.Handle;
    layer.subImage.imageRect.offset.x = 0;
    layer.subImage.imageRect.offset.y = 0;
    layer.subImage.imageRect.extent.width = swapchain.Width;
    layer.subImage.imageRect.extent.height = swapchain.Height;
    layer.size.width = scaleFactor;
    layer.size.height = layer.size.width * swapchain.Height / swapchain.Width;
    layer.subImage.imageArrayIndex = 0;
    layer.pose = cursorPose;
}
