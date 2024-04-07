/***************************************************************************************************

Filename    :   UILayer.h

Content     :   Utility class for creating interactive Android UI windows that
                are displayed in the VR environment.

                The UILayer manages two things:
                  1. a virtual display window that is created and managed by the
                      Android OS. This window is used to display the UI.
                  2. A surface-backed XrSwapchain that the virtual window renders into.

                From Android's perspective, it's rendering a group of views to a secondary
                display, while in reality it's rendering to a surface that is composited
                into the VR environment.

                UILayer will translate clicks on the VR layer into touch events on the corresponding
                Android window.

                This method is relatively efficient, but it must be used carefully. It
                will only render the UI when the UI has changed; however, it will continue rendering
                when the UI is not visible if there are updates. Additionally, while the UIs I am
                using are small enough that < 10MiB in RAM/GPU memory respectively, it could be
                a dumb, costly use of GPU resources to allocate huge, high-resolution UIs on init
                that are rarely used.

                Therefore, be careful on the front-end: don't create UIs that animate/update while
                they aren't visible, and don't create UIs that are too large. If these are needed,
                use another technique.

                (note: if rendering expensive UIs for occasional use is ever necessary, note that,
                because CitraVR is made of compositor layers on top of a compositor-rendered
                background, there is 0 risk of juddering due to missed frame updates. Therefore,
                you can get awaay with something most VR apps cannot: you can miss a bunch of
                frames in order to do an expensive one-time operation on the render thread in the
                frame loop -- like allocate a swapchain and construct a UI over a multi-frame period
                (dropping all frames in the process) -- with 0 risk of judder. If you are certain
                you can successfully de-init an expensive UI after it's used without any dangling
                resources, just do that instead of having it persist from init. The
                TryCreateSwapchain() paradigm is designed to support this, though you need to
                add a de-init function)

Authors     :   Amanda M. Watson
License     :   Licensed under GPLv3 or any later version.
                Refer to the license.txt file included.

***************************************************************************************************/

#pragma once

#include "../OpenXR.h"
#include "../Swapchain.h"
#include "../utils/Common.h"

#include <string>

/*
================================================================================

 UILayer

================================================================================
*/
class UILayer {

public:
    /** Constructor.
     * @param className: the class name of the Java class representing the UI layer.
     *        This class should subclass org.citra.citra_emu.ui.VrUILayer.
     * @param position: position of the layer
     * @param orientation: orientation of the layer
     * @param jni: the JNI environment. Should be attached to the current thread
     * @param activity object: reference to the current activity. Used to get
     * the class information for UILayer
     * @param session a valid XrSession
     */
    UILayer(const std::string& className, const XrVector3f&& position,
            const XrQuaternionf&& orientation, JNIEnv* jni, jobject activityObject,
            const XrSession& session);
    ~UILayer();

    /** Called once-per-frame. Populates the given layer list with a single layer
     *  representing the UI layer.
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

    /** Forwards the click event to the corresponding position on the Android UI window.
     *  @param pos2d the 2D position of the click in the Android display coordinate
     *  system.
     *  @param type the type of the click event. 0 for down, 1 for up, 2 for movement (while cursor
     *  is already pressed).
     */
    void SendClickToUI(const XrVector2f& pos2d, const int type);

    void SetPanelWithPose(const XrPosef& pose);

    bool IsMenuBackgroundSelected() const;

private:
    int Init(const std::string& className, const jobject activityObject, const XrVector3f& position,
             const XrSession& session);
    void Shutdown();

    /** Creates the swapchain.
     * @return 0 on success, error code less than 0 on failure.
     */
    int CreateSwapchain();

    const XrSession mSession;
    Swapchain       mSwapchain;

    XrPosef mPanelFromWorld;

    //============================
    // JNI objects
    JNIEnv* mEnv             = nullptr;
    jclass  mVrUILayerClass  = nullptr;
    jobject mVrUILayerObject = nullptr;
    jobject mSurface         = nullptr;

    //============================
    // JNI methods

    // Note: if we're using a view within a window as the layer (as opposed to
    // the decorView representing an entire window, it's important to accont for
    // the x, y offset of the view within the window, in case there are things
    // like window decorations or status bars.
    jmethodID mGetBoundsMethodID                = nullptr;
    jmethodID mSendClickToUIMethodID            = nullptr;
    jmethodID mSetSurfaceMethodId               = nullptr;
    jmethodID mIsMenuBackgroundSelectedMethodId = nullptr;
};
