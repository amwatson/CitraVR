#pragma once

#include "UILayer.h"

class RibbonLayer : public UILayer {
public:
    RibbonLayer(const XrVector3f&& position, const XrQuaternionf&& orientation, JNIEnv* jni,
                jobject activityObject, const XrSession& session);

    bool IsMenuBackgroundSelected() const;
    void SetPanelFromController(const XrVector3f& controllerPosition);
    void SetPanelFromThumbstick(const float thumbstickY);
    void SetPanelWithPose(const XrPosef& pose);

    const XrPosef& GetPose() const { return mPanelFromWorld; }

private:
    const XrPosef mInitialPose;
    jmethodID     mIsMenuBackgroundSelectedMethodId = nullptr;
};
