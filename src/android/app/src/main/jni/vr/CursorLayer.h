/*******************************************************************************

Filename    :   CursorLayer.h
Content     :   Renders a cursor
Authors     :   Amanda M. Watson
License     :   Licensed under GPLv2 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#pragma once

#include "Swapchain.h"

#include <jni.h> // needed for openxr_platform.h when XR_USE_PLATFORM_ANDROID is defined

#include <EGL/egl.h>

#define XR_USE_GRAPHICS_API_OPENGL_ES 1
#define XR_USE_PLATFORM_ANDROID       1

#include <openxr/openxr_platform.h>

#include <array>
#include <vector>

/*
================================================================================

CursorLayer

================================================================================
*/

class CursorLayer
{
public:
    /** The type of cursor to render.
     *  Determines the color of the cursor.
     **/
    enum class CursorType
    {
        CURSOR_TYPE_NORMAL,
        CURSOR_TYPE_TOP_PANEL,
        NUM_CURSOR_TYPES
    };
    /**
     * @param session a valid XR Session
     */
    CursorLayer(const XrSession& session);
    /** Called once-per-frame. Populates the given layer descriptor to show the
     * cursor, rendered at the given position.
     *
     *  @param space the XrSpace this layer should be positioned with. The
     * center of the layer is placed in the center of the FOV.
     *  @param layer the layer descriptor
     *  @param cursorPose position of the cursor, the specified reference space
     *  @param scaleFactor the scale factor to apply to the cursor
     *  @param cursorType the type of cursor to render
     */
    void Frame(const XrSpace& space, XrCompositionLayerQuad& layer,
               const XrPosef& cursorPose, const float scaleFactor,
               const CursorType& cursorType) const;

private:
    /** Helper class to manage the cursor image. */
    class CursorImage
    {
    public:
        /** Create a new cursor image.
         *
         *  @param session a valid XR Session
         *  @param colorRGB the background color of the cursor, in RGB (0-255)
         */
        CursorImage(const XrSession& session, const uint8_t colorRGB[3]);
        ~CursorImage();

        /** Get the swapchain used to render the cursor. */
        const Swapchain& GetSwapchain() const { return mSwapchain; }

    private:
        Swapchain                                mSwapchain;
        std::vector<XrSwapchainImageOpenGLESKHR> mSwapchainImages;
    };

    std::array<CursorImage, static_cast<size_t>(CursorType::NUM_CURSOR_TYPES)>
        mCursorImages;
};
