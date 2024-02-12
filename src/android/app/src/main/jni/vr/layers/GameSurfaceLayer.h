/*******************************************************************************

Filename    :   GameSurfaceLayer.cpp

Content     :   Handles the projection of the "Game Surface" panels into XR.
                Includes the "top" panel (stereo game screen) and the "bottom"
                panel (mono touchpad).

                The game surface is an Android surface(/ANativeWindow) that is
                presented to the Citra render code as if it were the
                SurfaceTexture of the main window. This allows VR and non-VR
                rendering to be identical from Citra's perspective.

                The screen is rendered in "portrait mode", meaning the top
                (video) panel is rendered above the bottom (touchpad) panel on
                the same surface. However, because the screen is also rendered
                (left-right) stereo, the final output will be slightly wider
                than it is tall.

                                        Game Surface
                +--------------------------------------------------------+
                | +-------------------------++-------------------------+ |
                | |                         ||                         | |
                | |           (L)           ||           (R)           | |
                | |         Top Panel       ||        Top Panel        | |
          480px | |         400x240px       ||        400x240px        | |
           tall | +-------------------------++-------------------------+ |
                |          +----------------++----------------+          |
                |          |                ||                |          |
                |          |       (L)      ||       (R)      |          |
                |          |  Bottom Panel  ||  Bottom Panel  |          |
                |          |    320x240px   ||    320x240px   |          |
                |          |                ||                |          |
                |          +----------------++----------------+          |
                +--------------------------------------------------------+
                                       800px wide

                The top panel is rendered as a stereo image, with the left
                By default, the top panel is a flat quad, placed in the center
                of the user's view for optimal clarity. There is also a debug
                option to render the top panel as a cylinder,
                which can have greater density/clarity in the center and create
                a greater sense of depth due to paralax, but can also have more
                stereo distortion, especially with wider IPDs and higher stereo
                depths. My IPD is really narrow, so almost everything works
                except at the highest stereo depths.

Authors     :   Amanda M. Watson
License     :   Licensed under GPLv3 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/
#pragma once

#include "../OpenXR.h"
#include "../Swapchain.h"
#include "../utils/Common.h"
#include "core/3ds.h" // for 3ds screen sizes.

#include <jni.h>

#include <string>
#include <vector>

/*
================================================================================

 GameSurfaceLayer

================================================================================
*/
class GameSurfaceLayer {

public:
    static constexpr float DEFAULT_QUAD_DENSITY                   = 240;
    static constexpr float DEFAULT_CYLINDER_RADIUS                = 2.0f;
    static constexpr float DEFAULT_CYLINDER_CENTRAL_ANGLE_DEGREES = 55.0f;

    /** Constructor.
     * @param position: position of the layer, in world space
     * @param jni: the JNI environment. Should be attached to the current thread
     * @param activity object: reference to the current activity. Used to get
     * the class information for gameSurfaceClass
     * @param session a valid XrSession
     */
    GameSurfaceLayer(const XrVector3f&& position, JNIEnv* jni, jobject activityObject,
                     const XrSession& session, const uint32_t resolutionFactor);
    ~GameSurfaceLayer();

    /** Called on resume. Sets the surface in the native rendering library.
     * Overrides the normal surface passed by Citra
     */
    void SetSurface() const;

    /** Called once-per-frame. Populates the layer list to show the
     *  top and bottom panels as two separate layers.
     *
     *  @param space the XrSpace this layer should be positioned with. The
     * center of the layer is placed in the center of the FOV.
     *  @param layers the array of layers to populate
     *  @param layerCount the layer count passed to XrEndFrame. This is incremented by
     *  the number of layers added by this function.
     */
    void Frame(const XrSpace& space, std::vector<XrCompositionLayer>& layers,
               uint32_t& layerCount) const;

    /** Given an origin, direction of a ray,
     *  returns the coordinates of where the ray will intersects
     *  with the UI layer. This is used to render the controller cursor
     *  and send clicks to the Android window coordinates.
     *
     *  @param start origin of the ray
     *  @param end the destination point of the ray
     *  @param result2d stores the 2D result on success. The 2D result is the 2D
     *   location of the intersection (cursor) in the Android display coordinate
     *   system. This is used to send input events to the window.
     *  @param result3d store the 3D result on success. The 3d result is the
     *   position where the layer and the ray intersect, in the target reference
     *   space. This is used to render the cursor.
     *
     *  @return true if the ray and the plane intersected and the position is
     *   within the layer's window bounds, false otherwise.
     *
     * Note: assumes viewer is looking down the -Z axis.
     */
    bool GetRayIntersectionWithPanel(const XrVector3f& start,
                                     const XrVector3f& end,
                                     XrVector2f&       result2d,
                                     XrPosef&          result3d) const;
    bool GetRayIntersectionWithPanelTopPanel(const XrVector3f& start,
                                             const XrVector3f& end,
                                             XrVector2f&       result2d,
                                             XrPosef&          result3d) const;
    void SetTopPanelFromController(const XrVector3f& controllerPosition);

    void SetTopPanelFromThumbstick(const float thumbstickY);

private:
    int  Init(const jobject activityObject, const XrVector3f& position, const XrSession& session);
    void Shutdown();

    /** Creates the swapchain.
     */
    void CreateSwapchain();

    static constexpr uint32_t SURFACE_WIDTH_UNSCALED =
        (NUM_EYES * std::max(Core::kScreenTopWidth, Core::kScreenBottomWidth)) - 300;
    static constexpr uint32_t SURFACE_HEIGHT_UNSCALED =
        (Core::kScreenTopHeight + Core::kScreenBottomHeight);

    // Width and height should both be even numbers, as the swapchain will
    // be split twice: once (horizontally) for stereo views, and once
    // (vertically) for the upper/lower screen.
    static_assert((SURFACE_WIDTH_UNSCALED % 2) == 0, "Swapchain width must be a multiple of 2");
    static_assert((SURFACE_HEIGHT_UNSCALED % 2) == 0, "Swapchain height must be a multiple of 2");

    const XrSession mSession;
    Swapchain       mSwapchain;

    XrPosef mTopPanelFromWorld;
    XrPosef mLowerPanelFromWorld;

    // Density scale for surface. Citra's auto-scale sets this as the internal
    // resolution.
    const uint32_t mResolutionFactor;

    // EXPERIMENTAL: When true, the top screen + its extents
    // (previously-unseen parts of the scene) are projected onto a 275-degree
    // cylinder. Note that the perceived resolution is lower,
    // as the output image is larger, both in texture size and perceived layer
    // size.
    //
    // Rendering a higher-resolution image would likely require
    // performance optimizations to avoid maxing out the GPU, e.g.:
    //   - Multiview (requires a merged Citra/CitraVR renderer)
    //   - Rendering the top-screen and bottom screen separately.
    const uint32_t mImmersiveMode;

    //============================
    // JNI objects
    JNIEnv* mEnv                = nullptr;
    jobject mActivityObject     = nullptr;
    jclass  mVrGameSurfaceClass = nullptr;
    jobject mSurface            = nullptr;
};
