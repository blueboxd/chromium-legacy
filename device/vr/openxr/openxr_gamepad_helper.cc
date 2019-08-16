// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_gamepad_helper.h"

#include "device/vr/openxr/openxr_util.h"

namespace device {

XrResult OpenXrGamepadHelper::CreateOpenXrGamepadHelper(
    XrInstance instance,
    XrSession session,
    XrSpace local_space,
    std::unique_ptr<OpenXrGamepadHelper>* helper) {
  XrResult xr_result;

  std::unique_ptr<OpenXrGamepadHelper> new_helper =
      std::make_unique<OpenXrGamepadHelper>(session, local_space);

  // This map is used to store bindings for different kinds of interaction
  // profiles. This allows the runtime to choose a different input sources based
  // on availability.
  std::map<XrPath, std::vector<XrActionSuggestedBinding>> bindings;
  DCHECK(new_helper->controllers_.size() ==
         new_helper->active_action_sets_.size());
  for (size_t i = 0; i < new_helper->controllers_.size(); i++) {
    RETURN_IF_XR_FAILED(new_helper->controllers_[i].Initialize(
        static_cast<OpenXrControllerType>(i), instance, session, &bindings));

    new_helper->active_action_sets_[i].actionSet =
        new_helper->controllers_[i].GetActionSet();
    new_helper->active_action_sets_[i].subactionPath = XR_NULL_PATH;
  }

  for (auto it = bindings.begin(); it != bindings.end(); it++) {
    XrInteractionProfileSuggestedBinding profile_suggested_bindings = {
        XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    profile_suggested_bindings.interactionProfile = it->first;
    profile_suggested_bindings.suggestedBindings = it->second.data();
    profile_suggested_bindings.countSuggestedBindings = it->second.size();

    RETURN_IF_XR_FAILED(xrSuggestInteractionProfileBindings(
        instance, &profile_suggested_bindings));
  }

  std::vector<XrActionSet> action_sets(new_helper->controllers_.size());
  for (size_t i = 0; i < new_helper->controllers_.size(); i++) {
    action_sets[i] = new_helper->controllers_[i].GetActionSet();
  }

  XrSessionActionSetsAttachInfo attach_info = {
      XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
  attach_info.countActionSets = action_sets.size();
  attach_info.actionSets = action_sets.data();
  RETURN_IF_XR_FAILED(xrAttachSessionActionSets(session, &attach_info));

  *helper = std::move(new_helper);
  return xr_result;
}

OpenXrGamepadHelper::OpenXrGamepadHelper(XrSession session, XrSpace local_space)
    : session_(session), local_space_(local_space) {}

OpenXrGamepadHelper::~OpenXrGamepadHelper() = default;

mojom::XRGamepadDataPtr OpenXrGamepadHelper::GetGamepadData(
    XrTime predicted_display_time) {
  XrActionsSyncInfo sync_info = {XR_TYPE_ACTIONS_SYNC_INFO};
  sync_info.countActiveActionSets = active_action_sets_.size();
  sync_info.activeActionSets = active_action_sets_.data();
  if (XR_FAILED(xrSyncActions(session_, &sync_info)))
    return nullptr;

  mojom::XRGamepadDataPtr gamepad_data_ptr = mojom::XRGamepadData::New();
  for (const OpenXrController& controller : controllers_) {
    mojom::XRGamepadPtr gamepad_ptr = mojom::XRGamepad::New();
    gamepad_ptr->controller_id = controller.GetID();
    gamepad_ptr->timestamp = base::TimeTicks::Now();
    gamepad_ptr->hand = controller.GetHandness();

    std::vector<mojom::XRGamepadButtonPtr> buttons =
        controller.GetWebVrButtons();
    if (buttons.empty())
      continue;
    gamepad_ptr->buttons = std::move(buttons);

    std::vector<double> axes = controller.GetWebVrAxes();
    if (axes.empty())
      continue;
    gamepad_ptr->axes = std::move(axes);

    gamepad_ptr->pose =
        controller.GetPose(predicted_display_time, local_space_);
    if (!gamepad_ptr->pose)
      continue;
    gamepad_ptr->can_provide_position = gamepad_ptr->pose->position.has_value();
    gamepad_ptr->can_provide_orientation =
        gamepad_ptr->pose->orientation.has_value();

    gamepad_data_ptr->gamepads.push_back(std::move(gamepad_ptr));
  }

  return gamepad_data_ptr;
}

}  // namespace device
