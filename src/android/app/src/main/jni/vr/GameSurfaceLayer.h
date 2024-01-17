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
License     :   Licensed under GPLv2 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/
#pragma once

#include "../../../../../../core/3ds.h" // for 3ds screen sizes.
#include "OpenXR.h"
#include "Swapchain.h"
#include "utils/Common.h"

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
    // Density scale for surface. Citra's auto-scale sets this as the internal
    // resolution.
    static constexpr uint32_t SCALE_FACTOR = 5;
    static constexpr float DEFAULT_QUAD_DENSITY = 240 * static_cast<float>(SCALE_FACTOR);
    static constexpr float DEFAULT_CYLINDER_RADIUS = 2.0f;
    static constexpr float DEFAULT_CYLINDER_CENTRAL_ANGLE_DEGREES = 55.0f;

    /** Constructor.
     * @param position: position of the layer, in world space
     * @param activity object: reference to the current activity. Used to get
     * the class information for gameSurfaceClass
     * @param session a valid XrSession
     */
    GameSurfaceLayer(const XrVector3f&& position, JNIEnv* jni, jobject activityObject,
                     const XrSession& session);
    ~GameSurfaceLayer();

    /** Called on resume. Sets the surface in the native rendering library.
     * Overrides the normal surface passed by Citra
     */
    void SetSurface() const;

    /** Called once-per-frame. Populates the given layer descriptor to show the
     *  top and bottom panels as two separate layers.
     *
     *  @param space the XrSpace this layer should be positioned with. The
     * center of the layer is placed in the center of the FOV.
     *  @param layers the array of layers to populate
     *  @param layerCount the number of layers in the array
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
    bool GetRayIntersectionWithPanel(const XrVector3f& start, const XrVector3f& end,
                                     XrVector2f& result2d, XrPosef& result3d) const;
    bool GetRayIntersectionWithPanelTopPanel(const XrVector3f& start, const XrVector3f& end,
                                             XrVector2f& result2d, XrPosef& result3d) const;
    void SetTopPanelFromController(const XrVector3f& controllerPosition);

    void SetTopPanelFromThumbstick(const float thumbstickY);

private:
    int Init(const jobject activityObject, const XrVector3f& position, const XrSession& session);
    void Shutdown();

    /** Creates the swapchain.
     */
    void CreateSwapchain();

    static constexpr uint32_t SURFACE_WIDTH =
        (NUM_EYES * std::max(Core::kScreenTopWidth, Core::kScreenBottomWidth) * SCALE_FACTOR) -
        1500;
    static constexpr uint32_t SURFACE_HEIGHT =
        (Core::kScreenTopHeight + Core::kScreenBottomHeight) * SCALE_FACTOR;

    // Width and height should both be even numbers, as the swapchain will
    // be split twice: once (horizontally) for stereo views, and once
    // (vertically) for the upper/lower screen.
    static_assert((SURFACE_WIDTH % 2) == 0, "Swapchain width must be a multiple of 2");
    static_assert((SURFACE_HEIGHT % 2) == 0, "Swapchain height must be a multiple of 2");

    const XrSession session_;
    Swapchain swapchain_;

    XrPosef topPanelFromWorld_;
    XrPosef lowerPanelFromWorld_;

    //============================
    // JNI objects
    JNIEnv* env_ = nullptr;
    jobject activityObject_ = nullptr;
    jclass vrGameSurfaceClass_ = nullptr;
    jobject surface_ = nullptr;

    //============================
    // JNI methods
    jmethodID setSurfaceMethodID_ = nullptr;
};
