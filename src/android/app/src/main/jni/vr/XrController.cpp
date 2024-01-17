/*******************************************************************************


Filename    :   XrController.cpp
Content     :   XR-specific input handling.
Authors     :   Amanda M. Watson
License     :   Licensed under GPLv2 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#include "OpenXR.h"
#include "XrController.h"
#include "utils/LogUtils.h"

#include <vector>

#include <assert.h>

#if defined(DEBUG_INPUT_VERBOSE)
#define ALOG_INPUT_VERBOSE(...) ALOGD(__VAR_ARGS__)
#else
#define ALOG_INPUT_VERBOSE(...)
#endif

namespace {

XrAction CreateAction(XrActionSet actionSet, XrActionType type, const char* actionName,
                      const char* localizedName, int countSubactionPaths = 0,
                      XrPath* subactionPaths = nullptr) {
    ALOG_INPUT_VERBOSE("CreateAction %s, %d" actionName, countSubactionPaths);

    XrActionCreateInfo aci = {};
    aci.type = XR_TYPE_ACTION_CREATE_INFO;
    aci.next = nullptr;
    aci.actionType = type;
    if (countSubactionPaths > 0) {
        aci.countSubactionPaths = countSubactionPaths;
        aci.subactionPaths = subactionPaths;
    }
    strcpy(aci.actionName, actionName);
    strcpy(aci.localizedActionName, localizedName ? localizedName : actionName);
    XrAction action = XR_NULL_HANDLE;
    OXR(xrCreateAction(actionSet, &aci, &action));
    return action;
}

XrActionSuggestedBinding ActionSuggestedBinding(const XrInstance& instance, XrAction action,
                                                const char* bindingString) {
    XrActionSuggestedBinding asb;
    asb.action = action;
    XrPath bindingPath;
    OXR(xrStringToPath(instance, bindingString, &bindingPath));
    asb.binding = bindingPath;
    return asb;
}

XrSpace CreateActionSpace(const XrSession& session, XrAction poseAction, XrPath subactionPath) {
    XrActionSpaceCreateInfo asci = {};
    asci.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
    asci.action = poseAction;
    asci.poseInActionSpace.orientation.w = 1.0f;
    asci.subactionPath = subactionPath;
    XrSpace actionSpace = XR_NULL_HANDLE;
    OXR(xrCreateActionSpace(session, &asci, &actionSpace));
    return actionSpace;
}

} // anonymous namespace

InputStateStatic::InputStateStatic(const XrInstance& instance, const XrSession& session) {
    // Create action set.
    {
        XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(actionSetInfo.actionSetName, "citra_controls");
        strcpy(actionSetInfo.localizedActionSetName, "Citra Controls");
        actionSetInfo.priority = 2;
        OXR(xrCreateActionSet(instance, &actionSetInfo, &mActionSet));
    }
    mRightHandIndexTriggerAction = CreateAction(mActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT,
                                                "right_index_trigger", "Right Index Trigger");
    mLeftHandIndexTriggerAction = CreateAction(mActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT,
                                               "left_index_trigger", "Left Index Trigger");
    mLeftMenuButtonAction = CreateAction(mActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "menu", "Menu");
    mAButtonAction = CreateAction(mActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "a", "A button");
    mBButtonAction = CreateAction(mActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "b", "B button");
    mXButtonAction = CreateAction(mActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "x", "X button");
    mYButtonAction = CreateAction(mActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "y", "Y button");

    OXR(xrStringToPath(instance, "/user/hand/left", &mLeftHandSubactionPath));
    OXR(xrStringToPath(instance, "/user/hand/right", &mRightHandSubactionPath));
    XrPath handSubactionPaths[2] = {mLeftHandSubactionPath, mRightHandSubactionPath};

    mHandPoseAction = CreateAction(mActionSet, XR_ACTION_TYPE_POSE_INPUT, "aim_pose", nullptr, 2,
                                   handSubactionPaths);

    mThumbStickAction = CreateAction(mActionSet, XR_ACTION_TYPE_VECTOR2F_INPUT, "thumb_stick",
                                     nullptr, 2, handSubactionPaths);

    mThumbClickAction = CreateAction(mActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "thumb_click",
                                     nullptr, 2, handSubactionPaths);

    mSqueezeTriggerAction = CreateAction(mActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT,
                                         "squeeze_trigger", nullptr, 2, handSubactionPaths);

    XrPath interactionProfilePath = XR_NULL_PATH;
    OXR(xrStringToPath(instance, "/interaction_profiles/oculus/touch_controller",
                       &interactionProfilePath));

    // Create bindings for Quest controllers.
    {
        // Map bindings

        std::vector<XrActionSuggestedBinding> bindings;
        bindings.push_back(
            ActionSuggestedBinding(instance, mAButtonAction, "/user/hand/right/input/a/click"));
        bindings.push_back(
            ActionSuggestedBinding(instance, mBButtonAction, "/user/hand/right/input/b/click"));
        bindings.push_back(
            ActionSuggestedBinding(instance, mXButtonAction, "/user/hand/left/input/x/click"));
        bindings.push_back(
            ActionSuggestedBinding(instance, mYButtonAction, "/user/hand/left/input/y/click"));

        bindings.push_back(ActionSuggestedBinding(instance, mLeftHandIndexTriggerAction,
                                                  "/user/hand/left/input/trigger"));
        bindings.push_back(ActionSuggestedBinding(instance, mRightHandIndexTriggerAction,
                                                  "/user/hand/right/input/trigger"));
        bindings.push_back(
            ActionSuggestedBinding(instance, mHandPoseAction, "/user/hand/left/input/aim/pose"));
        bindings.push_back(
            ActionSuggestedBinding(instance, mHandPoseAction, "/user/hand/right/input/aim/pose"));
        bindings.push_back(ActionSuggestedBinding(instance, mLeftMenuButtonAction,
                                                  "/user/hand/left/input/menu/click"));
        bindings.push_back(ActionSuggestedBinding(instance, mThumbStickAction,
                                                  "/user/hand/left/input/thumbstick"));
        bindings.push_back(ActionSuggestedBinding(instance, mThumbStickAction,
                                                  "/user/hand/right/input/thumbstick"));

        bindings.push_back(ActionSuggestedBinding(instance, mThumbClickAction,
                                                  "/user/hand/right/input/thumbstick/click"));
        bindings.push_back(ActionSuggestedBinding(instance, mThumbClickAction,
                                                  "/user/hand/left/input/thumbstick/click"));

        bindings.push_back(ActionSuggestedBinding(instance, mSqueezeTriggerAction,
                                                  "/user/hand/right/input/squeeze/value"));
        bindings.push_back(ActionSuggestedBinding(instance, mSqueezeTriggerAction,
                                                  "/user/hand/left/input/squeeze/value"));

        XrInteractionProfileSuggestedBinding suggestedBindings = {};
        suggestedBindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
        suggestedBindings.interactionProfile = interactionProfilePath;
        suggestedBindings.suggestedBindings = &bindings[0];
        suggestedBindings.countSuggestedBindings = bindings.size();
        OXR(xrSuggestInteractionProfileBindings(instance, &suggestedBindings));

        // Attach to session
        XrSessionActionSetsAttachInfo attachInfo = {};
        attachInfo.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
        attachInfo.countActionSets = 1;
        attachInfo.actionSets = &mActionSet;
        OXR(xrAttachSessionActionSets(session, &attachInfo));
    }
}

InputStateStatic::~InputStateStatic() {
    if (mActionSet != XR_NULL_HANDLE) {
        OXR(xrDestroyActionSet(mActionSet));
    }
    OXR(xrDestroyAction(mLeftHandIndexTriggerAction));
    OXR(xrDestroyAction(mRightHandIndexTriggerAction));
    OXR(xrDestroyAction(mLeftMenuButtonAction));
    OXR(xrDestroyAction(mAButtonAction));
    OXR(xrDestroyAction(mBButtonAction));
    OXR(xrDestroyAction(mXButtonAction));
    OXR(xrDestroyAction(mYButtonAction));
    OXR(xrDestroyAction(mHandPoseAction));
    OXR(xrDestroyAction(mThumbStickAction));
    OXR(xrDestroyAction(mThumbClickAction));
    OXR(xrDestroyAction(mSqueezeTriggerAction));

    if (mLeftHandSpace != XR_NULL_HANDLE) {
        OXR(xrDestroySpace(mLeftHandSpace));
    }
    if (mRightHandSpace != XR_NULL_HANDLE) {
        OXR(xrDestroySpace(mRightHandSpace));
    }
}

XrActionStateBoolean SyncButtonState(const XrSession& session, const XrAction& action,
                                     const XrPath& subactionPath = XR_NULL_PATH) {
    XrActionStateGetInfo getInfo = {};
    getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
    getInfo.action = action;
    getInfo.subactionPath = subactionPath;

    XrActionStateBoolean state = {};
    state.type = XR_TYPE_ACTION_STATE_BOOLEAN;

    OXR(xrGetActionStateBoolean(session, &getInfo, &state));
    return state;
}

XrActionStateVector2f SyncVector2fState(const XrSession& session, const XrAction& action,
                                        const XrPath& subactionPath = XR_NULL_PATH) {
    XrActionStateGetInfo getInfo = {};
    getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
    getInfo.action = action;
    getInfo.subactionPath = subactionPath;

    XrActionStateVector2f state = {};
    state.type = XR_TYPE_ACTION_STATE_VECTOR2F;

    OXR(xrGetActionStateVector2f(session, &getInfo, &state));
    return state;
}

void InputStateFrame::SyncButtonsAndThumbSticks(
    const XrSession& session, const std::unique_ptr<InputStateStatic>& staticState) {
    assert(staticState != nullptr);
    XrActiveActionSet activeActionSet = {};
    activeActionSet.actionSet = staticState->mActionSet;
    activeActionSet.subactionPath = XR_NULL_PATH;

    XrActionsSyncInfo syncInfo = {};
    syncInfo.type = XR_TYPE_ACTIONS_SYNC_INFO;
    syncInfo.next = nullptr;
    syncInfo.countActiveActionSets = 1;
    syncInfo.activeActionSets = &activeActionSet;
    OXR(xrSyncActions(session, &syncInfo));

    // Sync button states
    mAButtonState = SyncButtonState(session, staticState->mAButtonAction);
    mBButtonState = SyncButtonState(session, staticState->mBButtonAction);
    mXButtonState = SyncButtonState(session, staticState->mXButtonAction);
    mYButtonState = SyncButtonState(session, staticState->mYButtonAction);

    mLeftMenuButtonState = SyncButtonState(session, staticState->mLeftMenuButtonAction);

    // Sync thumbstick states
    mThumbStickState[LEFT_CONTROLLER] = SyncVector2fState(session, staticState->mThumbStickAction,
                                                          staticState->mLeftHandSubactionPath);
    mThumbStickState[RIGHT_CONTROLLER] = SyncVector2fState(session, staticState->mThumbStickAction,
                                                           staticState->mRightHandSubactionPath);

    // Sync thumbstick click states
    mThumbStickClickState[LEFT_CONTROLLER] = SyncButtonState(
        session, staticState->mThumbClickAction, staticState->mLeftHandSubactionPath);
    mThumbStickClickState[RIGHT_CONTROLLER] = SyncButtonState(
        session, staticState->mThumbClickAction, staticState->mRightHandSubactionPath);

    // Sync index trigger states
    mIndexTriggerState[LEFT_CONTROLLER] = SyncButtonState(
        session, staticState->mLeftHandIndexTriggerAction, staticState->mLeftHandSubactionPath);
    mIndexTriggerState[RIGHT_CONTROLLER] = SyncButtonState(
        session, staticState->mRightHandIndexTriggerAction, staticState->mRightHandSubactionPath);

    // Sync squeeze trigger states
    mSqueezeTriggerState[LEFT_CONTROLLER] = SyncButtonState(
        session, staticState->mSqueezeTriggerAction, staticState->mLeftHandSubactionPath);
    mSqueezeTriggerState[RIGHT_CONTROLLER] = SyncButtonState(
        session, staticState->mSqueezeTriggerAction, staticState->mRightHandSubactionPath);

    if (staticState->mLeftHandSpace == XR_NULL_HANDLE) {
        staticState->mLeftHandSpace = CreateActionSpace(session, staticState->mHandPoseAction,
                                                        staticState->mLeftHandSubactionPath);
    }
    if (staticState->mRightHandSpace == XR_NULL_HANDLE) {
        staticState->mRightHandSpace = CreateActionSpace(session, staticState->mHandPoseAction,
                                                         staticState->mRightHandSubactionPath);
    }

    // get the active state and pose for the two comtrollers
    if (staticState->mLeftHandSpace != XR_NULL_HANDLE) {
        XrActionStateGetInfo getInfo = {.type = XR_TYPE_ACTION_STATE_GET_INFO,
                                        .action = staticState->mHandPoseAction,
                                        .subactionPath = staticState->mLeftHandSubactionPath};
        XrActionStatePose handPose = {.type = XR_TYPE_ACTION_STATE_POSE};
        OXR(xrGetActionStatePose(session, &getInfo, &handPose));
        mIsHandActive[LEFT_CONTROLLER] = handPose.isActive;
    }
    if (staticState->mRightHandSpace != XR_NULL_HANDLE) {
        XrActionStateGetInfo getInfo = {.type = XR_TYPE_ACTION_STATE_GET_INFO,
                                        .action = staticState->mHandPoseAction,
                                        .subactionPath = staticState->mRightHandSubactionPath};
        XrActionStatePose handPose = {.type = XR_TYPE_ACTION_STATE_POSE};
        OXR(xrGetActionStatePose(session, &getInfo, &handPose));
        mIsHandActive[RIGHT_CONTROLLER] = handPose.isActive;
    }
}

void InputStateFrame::SyncHandPoses(const XrSession& session,
                                    const std::unique_ptr<InputStateStatic>& staticState,
                                    const XrSpace& referenceSpace,
                                    const XrTime predictedDisplayTime) {
    OXR(xrLocateSpace(staticState->mRightHandSpace, referenceSpace, predictedDisplayTime,
                      &mHandPositions[InputStateFrame::RIGHT_CONTROLLER]));
    mIsHandActive[RIGHT_CONTROLLER] =
        (mHandPositions[InputStateFrame::RIGHT_CONTROLLER].locationFlags &
         XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0;

    OXR(xrLocateSpace(staticState->mLeftHandSpace, referenceSpace, predictedDisplayTime,
                      &mHandPositions[InputStateFrame::LEFT_CONTROLLER]));
    mIsHandActive[LEFT_CONTROLLER] =
        (mHandPositions[InputStateFrame::LEFT_CONTROLLER].locationFlags &
         XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0;

    // Determine preferred hand.
    {
        // First, determine which controllers are active
        const bool isLeftHandActive = mIsHandActive[LEFT_CONTROLLER];
        const bool isRightHandActive = mIsHandActive[RIGHT_CONTROLLER];

        // if only one controller is active, use that one
        if (isLeftHandActive && !isRightHandActive) {
            mPreferredHand = LEFT_CONTROLLER;
        } else if (!isLeftHandActive && isRightHandActive) {
            mPreferredHand = RIGHT_CONTROLLER;
        } else if (isLeftHandActive && isRightHandActive) {
            // if both controllers are active, use whichever one last pressed
            // the index trigger
            if (mIndexTriggerState[LEFT_CONTROLLER].changedSinceLastSync &&
                mIndexTriggerState[LEFT_CONTROLLER].currentState == 1) {
                mPreferredHand = LEFT_CONTROLLER;
            }
            if (mIndexTriggerState[RIGHT_CONTROLLER].changedSinceLastSync &&
                mIndexTriggerState[RIGHT_CONTROLLER].currentState == 1) {
                mPreferredHand = RIGHT_CONTROLLER;
            } else {
                // if neither controller has pressed the index trigger, use the
                // last active controller
            }
        } else {
            // if no controllers are active, use the last active controller
        }
    }
}
