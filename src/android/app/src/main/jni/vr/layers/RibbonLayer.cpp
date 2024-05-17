#include "RibbonLayer.h"

#include "../utils/JniClassNames.h"
#include "../utils/LogUtils.h"
#include "../utils/XrMath.h"

namespace {
constexpr float kInitialLowerPanelPitchInRadians = -MATH_FLOAT_PI / 4.0f; // -45 degrees in radians

float gPitchAdjustInRadians = kInitialLowerPanelPitchInRadians;

XrVector3f CalculatePanelPosition(const XrVector3f& viewerPosition,
                                  const XrVector3f& controllerPosition,
                                  float             sphereRadius) {
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

} // anonymous namespace

RibbonLayer::RibbonLayer(const XrVector3f&& position, const XrQuaternionf&& orientation,
                         JNIEnv* jni, jobject activityObject, const XrSession& session)
    : UILayer(VR::JniGlobalRef::gVrRibbonLayerClass, std::move(position), std::move(orientation),
              jni, activityObject, session)
    , mInitialPose({orientation, position}) {
    mIsMenuBackgroundSelectedMethodId =
        jni->GetMethodID(GetVrUILayerClass(), "isMenuBackgroundSelected", "()Z");
    if (mIsMenuBackgroundSelectedMethodId == nullptr) {
        FAIL("Could not find isMenuBackgroundSelected()");
    }
}

bool RibbonLayer::IsMenuBackgroundSelected() const {
    return GetEnv()->CallBooleanMethod(GetVrUILayerObject(), mIsMenuBackgroundSelectedMethodId);
}

// Goal is to rotate the lower panel to face the user, but with an initial bias of 45 degrees.
// The result is the lower panel being slightly tilted away from the user compared to the top panel,
// but comfortably readable at any angle.
// The rotational offset is done so that the top+bottom text comfortably fit into the user's FOV
// at high angles, so the user isn't craning their neck while reclining.
void RibbonLayer::SetPanelFromController(const XrVector3f& controllerPosition) {
    constexpr XrVector3f viewerPosition{0.0f, 0.0f, 0.0f};    // Viewer position at origin
    constexpr XrVector3f windowUpDirection{0.0f, 1.0f, 0.0f}; // Y is up
    // Arbitrary factor, chosen based on what change-in-pitch felt best
    // A higher factor will make the window pitch more aggressively with vertical displacement.
    constexpr float pitchAdjustmentFactor = 0.5f;

    // Calculate sphere radius based on panel position to viewer
    const float sphereRadius = XrMath::Vector3f::Length(mPanelFromWorld.position - viewerPosition);

    // Calculate new window position based on controller and sphere radius
    const XrVector3f windowPosition =
        CalculatePanelPosition(viewerPosition, controllerPosition, sphereRadius);

    // Limit vertical range to prevent the window from being too close to the viewer or the top
    // panel.
    if (windowPosition.z >= -0.5f) { return; }

    // Calculate the base rotation of the panel to face the user
    const XrQuaternionf baseRotation =
        CalculatePanelRotation(windowPosition, viewerPosition, windowUpDirection);

    // Calculate pitch adjustment based on vertical displacement from initial position
    const float verticalDisplacement = windowPosition.y - mInitialPose.position.y;
    const float pitchAdjustment      = verticalDisplacement * pitchAdjustmentFactor;
    // Clamp the new pitch to reasonable bounds (-45 to 90 degrees)
    const float newPitchRadians =
        std::clamp(-std::abs(pitchAdjustment), 0.0f, MATH_FLOAT_PI / 4.0f) -
        kInitialLowerPanelPitchInRadians / 2.0f;

    // Construct a quaternion for the pitch adjustment
    const XrQuaternionf pitchAdjustmentQuat =
        XrMath::Quatf::FromAxisAngle({1.0f, 0.0f, 0.0f}, newPitchRadians + gPitchAdjustInRadians);

    // Combine the base rotation with the pitch adjustment
    mPanelFromWorld = {baseRotation * pitchAdjustmentQuat, windowPosition};
}
static constexpr float kThumbstickSpeed = 0.010f;
// Use thumbstick to tilt the pitch of the panel
void RibbonLayer::SetPanelFromThumbstick(const float thumbstickY) {
    const float pitchAdjustInRadians = -thumbstickY * kThumbstickSpeed;
    mPanelFromWorld.orientation =
        XrMath::Quatf::FromAxisAngle({1.0f, 0.0f, 0.0f}, pitchAdjustInRadians) *
        mPanelFromWorld.orientation;

    gPitchAdjustInRadians += pitchAdjustInRadians;
}
