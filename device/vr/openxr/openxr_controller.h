// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_CONTROLLER_H_
#define DEVICE_VR_OPENXR_OPENXR_CONTROLLER_H_

#include <stdint.h>
#include <unordered_map>

#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/util/gamepad_builder.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

constexpr uint32_t kAxisDimensions = 2;

enum class OpenXrControllerType {
  kLeft = 0,
  kRight = 1,
  kCount = 2,
};

enum class OpenXrButtonType {
  kTrigger = 0,
  kTrackpad = 1,
  kGrip = 2,
  kMenu = 3,
  kMaxValue = 3,
};

enum class OpenXrAxisType {
  kTrackpad = 0,
  kThumbstick = 1,
  kMaxValue = 1,
};

class OpenXrController {
 public:
  OpenXrController();
  ~OpenXrController();

  XrResult Initialize(
      OpenXrControllerType type,
      XrInstance instance,
      XrSession session,
      std::map<XrPath, std::vector<XrActionSuggestedBinding>>* bindings);

  XrActionSet GetActionSet() const;
  uint32_t GetID() const;
  device::mojom::XRHandedness GetHandness() const;

  std::vector<mojom::XRGamepadButtonPtr> GetWebVrButtons() const;
  std::vector<double> GetWebVrAxes() const;
  mojom::VRPosePtr GetPose(XrTime predicted_display_time,
                           XrSpace local_space) const;

 private:
  // ActionButton struct is used to store all XrAction that is related to the
  // button. For example, we may need to query the analog value for button press
  // which require a seperate XrAction than the current boolean XrAction.
  struct ActionButton {
    XrAction press_action;
    ActionButton() : press_action(XR_NULL_HANDLE) {}
  };

  XrResult InitializeMicrosoftMotionControllers(
      XrInstance instance,
      const std::string& type_string,
      std::map<XrPath, std::vector<XrActionSuggestedBinding>>* bindings);

  XrResult CreateAction(
      XrInstance instance,
      XrActionType type,
      const char* interaction_profile_name,
      const std::string& binding_string,
      const std::string& action_name,
      XrAction* action,
      std::map<XrPath, std::vector<XrActionSuggestedBinding>>* bindings);

  template <typename T>
  XrResult QueryState(XrAction action, T* action_state) const {
    // this function should never be called because each valid XrActionState
    // has its own template function defined below.
    NOTREACHED();
    return XR_ERROR_ACTION_TYPE_MISMATCH;
  }

  template <>
  XrResult QueryState<XrActionStateBoolean>(
      XrAction action,
      XrActionStateBoolean* action_state) const {
    action_state->type = XR_TYPE_ACTION_STATE_BOOLEAN;
    XrActionStateGetInfo get_info = {XR_TYPE_ACTION_STATE_GET_INFO};
    get_info.action = action;
    return xrGetActionStateBoolean(session_, &get_info, action_state);
  }

  template <>
  XrResult QueryState<XrActionStateVector2f>(
      XrAction action,
      XrActionStateVector2f* action_state) const {
    action_state->type = XR_TYPE_ACTION_STATE_VECTOR2F;
    XrActionStateGetInfo get_info = {XR_TYPE_ACTION_STATE_GET_INFO};
    get_info.action = action;
    return xrGetActionStateVector2f(session_, &get_info, action_state);
  }

  template <>
  XrResult QueryState<XrActionStatePose>(
      XrAction action,
      XrActionStatePose* action_state) const {
    action_state->type = XR_TYPE_ACTION_STATE_POSE;
    XrActionStateGetInfo get_info = {XR_TYPE_ACTION_STATE_GET_INFO};
    get_info.action = action;
    return xrGetActionStatePose(session_, &get_info, action_state);
  }

  std::vector<double> GetAxis(OpenXrAxisType type) const;
  mojom::XRGamepadButtonPtr GetButton(OpenXrButtonType type) const;
  mojom::XRGamepadButtonPtr GetGamepadButton(
      const XrActionStateBoolean& action_state) const;

  OpenXrControllerType type_;
  XrSession session_;
  XrActionSet action_set_;
  XrAction grip_pose_action_;
  XrSpace grip_pose_space_;

  std::unordered_map<OpenXrButtonType, ActionButton> button_action_map_;
  std::unordered_map<OpenXrAxisType, XrAction> axis_action_map_;

  DISALLOW_COPY_AND_ASSIGN(OpenXrController);
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_CONTROLLER_H_
