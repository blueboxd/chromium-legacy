// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_gamepad_helper.h"

#include "device/vr/openxr/openxr_util.h"

namespace device {

XrResult OpenXrGamepadHelper::GetOpenXrGamepadHelper(
    XrInstance instance,
    XrSession session,
    XrSpace local_space,
    std::unique_ptr<OpenXrGamepadHelper>* helper) {
  XrResult xr_result;
  // This map is used to store bindings for different kinds of interaction
  // profiles. This allows the runtime to choose a different input sources based
  // on availability.
  std::map<XrPath, std::vector<XrActionSuggestedBinding>> bindings;
  std::unique_ptr<OpenXrGamepadHelper> new_helper =
      std::make_unique<OpenXrGamepadHelper>(session, local_space);
  for (size_t i = 0; i < new_helper->controllers_.size(); i++) {
    xr_result = new_helper->controllers_[i].Initialize(
        static_cast<OpenXrControllerType>(i), instance, session, &bindings);
    RETURN_IF_XR_FAILED(xr_result);
  }

  for (auto it = bindings.begin(); it != bindings.end(); it++) {
    XrInteractionProfileSuggestedBinding profile_suggested_bindings = {
        XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    profile_suggested_bindings.interactionProfile = it->first;
    profile_suggested_bindings.suggestedBindings = it->second.data();
    profile_suggested_bindings.countSuggestedBindings = it->second.size();

    xr_result = xrSetInteractionProfileSuggestedBindings(
        session, &profile_suggested_bindings);
  }

  RETURN_IF_XR_FAILED(xr_result);

  *helper = std::move(new_helper);
  return xr_result;
}

OpenXrGamepadHelper::OpenXrGamepadHelper(XrSession session, XrSpace local_space)
    : session_(session), local_space_(local_space) {}

OpenXrGamepadHelper::~OpenXrGamepadHelper() = default;

mojom::XRGamepadDataPtr OpenXrGamepadHelper::GetGamepadData(
    XrTime predicted_display_time) {
  XrResult xr_result;

  std::array<XrActiveActionSet, 2> active_action_set_arr;
  for (size_t i = 0; i < controllers_.size(); i++) {
    active_action_set_arr[i] = {XR_TYPE_ACTIVE_ACTION_SET};
    active_action_set_arr[i].actionSet = controllers_[i].GetActionSet();
    active_action_set_arr[i].subactionPath = XR_NULL_PATH;
  }
  xr_result = xrSyncActionData(session_, controllers_.size(),
                               active_action_set_arr.data());
  if (XR_FAILED(xr_result))
    return nullptr;

  mojom::XRGamepadDataPtr gamepad_data_ptr = mojom::XRGamepadData::New();
  for (OpenXrController& controller : controllers_) {
    mojom::XRGamepadPtr gamepad_ptr = mojom::XRGamepad::New();
    gamepad_ptr->controller_id = controller.GetID();
    gamepad_ptr->timestamp = base::TimeTicks::Now();
    gamepad_ptr->hand = controller.GetHandness();

    // GetButtons, GetAxes and GetPose may return success codes >= 0, including
    // XR_SUCCESS and XR_STATE_UNAVAILABLE. We should continue to populate the
    // data only on XR_SUCCESS and ignore other success codes.
    if (!XR_UNQUALIFIED_SUCCESS(
            controller.GetWebVrButtons(&(gamepad_ptr->buttons)))) {
      continue;
    }
    if (!XR_UNQUALIFIED_SUCCESS(
            controller.GetAllWebVrAxes(&(gamepad_ptr->axes)))) {
      continue;
    }
    if (!XR_UNQUALIFIED_SUCCESS(controller.GetPose(
            predicted_display_time, local_space_, gamepad_ptr.get()))) {
      continue;
    }

    if (XR_UNQUALIFIED_SUCCESS(xr_result)) {
      gamepad_data_ptr->gamepads.push_back(std::move(gamepad_ptr));
    }
  }

  return gamepad_data_ptr;
}

}  // namespace device
