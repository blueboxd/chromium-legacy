// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_controller.h"

#include <stdint.h>

#include "base/logging.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/mojom/vr_service.mojom.h"

namespace device {

namespace {

constexpr char kMicrosoftInteractionProfileName[] =
    "/interaction_profiles/microsoft/motion_controller";

const char* GetStringFromType(OpenXrControllerType type) {
  switch (type) {
    case OpenXrControllerType::kLeft:
      return "left";
    case OpenXrControllerType::kRight:
      return "right";
    default:
      NOTREACHED();
      return "";
  }
}

}  // namespace

OpenXrController::OpenXrController()
    : type_(OpenXrControllerType::kCount),  // COUNT refers to invalid.
      session_(XR_NULL_HANDLE),
      action_set_(XR_NULL_HANDLE),
      grip_pose_action_{XR_NULL_HANDLE},
      grip_pose_space_(XR_NULL_HANDLE) {}

OpenXrController::~OpenXrController() {
  // We don't need to destroy all of the actions because destroying an
  // action set destroys all actions that are part of the set.

  if (action_set_ != XR_NULL_HANDLE) {
    xrDestroyActionSet(action_set_);
  }
  if (grip_pose_space_ != XR_NULL_HANDLE) {
    xrDestroySpace(grip_pose_space_);
  }
}

XrResult OpenXrController::Initialize(
    OpenXrControllerType type,
    XrInstance instance,
    XrSession session,
    std::map<XrPath, std::vector<XrActionSuggestedBinding>>* bindings) {
  type_ = type;
  session_ = session;
  std::string type_string = GetStringFromType(type_);

  std::string action_set_name = type_string + "_action_set";
  XrActionSetCreateInfo action_set_create_info = {
      XR_TYPE_ACTION_SET_CREATE_INFO};

  errno_t error =
      strcpy_s(action_set_create_info.actionSetName, action_set_name.c_str());
  DCHECK(!error);
  error = strcpy_s(action_set_create_info.localizedActionSetName,
                   action_set_name.c_str());
  DCHECK(!error);

  XrResult xr_result;

  RETURN_IF_XR_FAILED(
      xrCreateActionSet(instance, &action_set_create_info, &action_set_));

  RETURN_IF_XR_FAILED(
      InitializeMicrosoftMotionControllers(instance, type_string, bindings));

  XrActionSpaceCreateInfo action_space_create_info = {};
  action_space_create_info.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
  action_space_create_info.action = grip_pose_action_;
  action_space_create_info.subactionPath = XR_NULL_PATH;
  action_space_create_info.poseInActionSpace = PoseIdentity();
  RETURN_IF_XR_FAILED(xrCreateActionSpace(session_, &action_space_create_info,
                                          &grip_pose_space_));
  return xr_result;
}

XrResult OpenXrController::InitializeMicrosoftMotionControllers(
    XrInstance instance,
    const std::string& type_string,
    std::map<XrPath, std::vector<XrActionSuggestedBinding>>* bindings) {
  XrResult xr_result;

  const std::string binding_prefix = "/user/hand/" + type_string + "/input/";
  const std::string name_prefix = type_string + "_controller_";

  RETURN_IF_XR_FAILED(CreateAction(
      instance, XR_ACTION_TYPE_BOOLEAN_INPUT, kMicrosoftInteractionProfileName,
      binding_prefix + "trackpad/click", name_prefix + "trackpad_button_press",
      &(button_action_map_[OpenXrButtonType::kTrackpad].press_action),
      bindings));

  RETURN_IF_XR_FAILED(CreateAction(
      instance, XR_ACTION_TYPE_BOOLEAN_INPUT, kMicrosoftInteractionProfileName,
      binding_prefix + "trigger/value", name_prefix + "trigger_button_press",
      &(button_action_map_[OpenXrButtonType::kTrigger].press_action),
      bindings));

  // In OpenXR, this input is called 'squeeze'. However, the rest of Chromium
  // uses the term 'grip' for this button, so the OpenXrButtonType is named
  // kGrip for consistency.
  RETURN_IF_XR_FAILED(CreateAction(
      instance, XR_ACTION_TYPE_BOOLEAN_INPUT, kMicrosoftInteractionProfileName,
      binding_prefix + "squeeze/click", name_prefix + "squeeze_button_press",
      &(button_action_map_[OpenXrButtonType::kGrip].press_action), bindings));

  RETURN_IF_XR_FAILED(CreateAction(
      instance, XR_ACTION_TYPE_BOOLEAN_INPUT, kMicrosoftInteractionProfileName,
      binding_prefix + "menu/click", name_prefix + "menu_button_press",
      &(button_action_map_[OpenXrButtonType::kMenu].press_action), bindings));

  RETURN_IF_XR_FAILED(CreateAction(
      instance, XR_ACTION_TYPE_VECTOR2F_INPUT, kMicrosoftInteractionProfileName,
      binding_prefix + "trackpad", name_prefix + "trackpad_axis",
      &(axis_action_map_[OpenXrAxisType::kTrackpad]), bindings));

  RETURN_IF_XR_FAILED(CreateAction(
      instance, XR_ACTION_TYPE_VECTOR2F_INPUT, kMicrosoftInteractionProfileName,
      binding_prefix + "thumbstick", name_prefix + "thumbstick_axis",
      &(axis_action_map_[OpenXrAxisType::kThumbstick]), bindings));

  RETURN_IF_XR_FAILED(
      CreateAction(instance, XR_ACTION_TYPE_POSE_INPUT,
                   kMicrosoftInteractionProfileName, binding_prefix + "grip",
                   name_prefix + "grip_pose", &grip_pose_action_, bindings));

  return xr_result;
}

uint32_t OpenXrController::GetID() const {
  return static_cast<uint32_t>(type_);
}

device::mojom::XRHandedness OpenXrController::GetHandness() const {
  switch (type_) {
    case OpenXrControllerType::kLeft:
      return device::mojom::XRHandedness::LEFT;
    case OpenXrControllerType::kRight:
      return device::mojom::XRHandedness::RIGHT;
    default:
      // LEFT controller and RIGHT controller are currently the only supported
      // controllers. In the future, other controllers such as sound (which
      // does not have a handedness) will be added here.
      NOTREACHED();
      return device::mojom::XRHandedness::NONE;
  }
}

std::vector<mojom::XRGamepadButtonPtr> OpenXrController::GetWebVrButtons()
    const {
  std::vector<mojom::XRGamepadButtonPtr> buttons;

  constexpr uint32_t kNumButtons =
      static_cast<uint32_t>(OpenXrButtonType::kMaxValue) + 1;
  for (uint32_t i = 0; i < kNumButtons; i++) {
    mojom::XRGamepadButtonPtr mojo_button_ptr =
        GetButton(static_cast<OpenXrButtonType>(i));
    if (!mojo_button_ptr) {
      return {};
    }

    buttons.push_back(std::move(mojo_button_ptr));
  }

  return buttons;
}

mojom::XRGamepadButtonPtr OpenXrController::GetButton(
    OpenXrButtonType type) const {
  XrActionStateBoolean press_state_bool = {XR_TYPE_ACTION_STATE_BOOLEAN};
  if (XR_FAILED(QueryState(button_action_map_.at(type).press_action,
                           &press_state_bool)) ||
      !press_state_bool.isActive) {
    return nullptr;
  }

  return GetGamepadButton(press_state_bool);
}

mojom::XRGamepadButtonPtr OpenXrController::GetGamepadButton(
    const XrActionStateBoolean& action_state) const {
  mojom::XRGamepadButtonPtr ret = mojom::XRGamepadButton::New();
  bool button_pressed = action_state.currentState;
  ret->touched = button_pressed;
  ret->pressed = button_pressed;
  ret->value = button_pressed ? 1.0 : 0.0;

  return ret;
}

std::vector<double> OpenXrController::GetWebVrAxes() const {
  std::vector<double> axes;

  constexpr uint32_t kNumAxes =
      static_cast<uint32_t>(OpenXrAxisType::kMaxValue) + 1;
  for (uint32_t i = 0; i < kNumAxes; i++) {
    std::vector<double> axis = GetAxis(static_cast<OpenXrAxisType>(i));
    if (axis.empty()) {
      return {};
    }

    DCHECK(axis.size() == kAxisDimensions);
    axes.insert(axes.end(), axis.begin(), axis.end());
  }

  return axes;
}

std::vector<double> OpenXrController::GetAxis(OpenXrAxisType type) const {
  XrActionStateVector2f axis_state_v2f = {XR_TYPE_ACTION_STATE_VECTOR2F};
  if (XR_FAILED(QueryState(axis_action_map_.at(type), &axis_state_v2f)) ||
      !axis_state_v2f.isActive) {
    return {};
  }

  return {axis_state_v2f.currentState.x, axis_state_v2f.currentState.y};
}

mojom::VRPosePtr OpenXrController::GetPose(XrTime predicted_display_time,
                                           XrSpace local_space) const {
  XrActionStatePose grip_state_pose = {XR_TYPE_ACTION_STATE_POSE};
  if (XR_FAILED(QueryState(grip_pose_action_, &grip_state_pose)) ||
      !grip_state_pose.isActive) {
    return nullptr;
  }

  XrSpaceVelocity local_from_grip_speed = {XR_TYPE_SPACE_VELOCITY};
  XrSpaceLocation local_from_grip = {XR_TYPE_SPACE_LOCATION};
  local_from_grip.next = &local_from_grip_speed;
  if (XR_FAILED(xrLocateSpace(grip_pose_space_, local_space,
                              predicted_display_time, &local_from_grip))) {
    return nullptr;
  }

  mojom::VRPosePtr pose = mojom::VRPose::New();

  if (local_from_grip.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
    pose->position = gfx::Point3F(local_from_grip.pose.position.x,
                                  local_from_grip.pose.position.y,
                                  local_from_grip.pose.position.z);
  }

  if (local_from_grip.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) {
    pose->orientation = gfx::Quaternion(
        local_from_grip.pose.orientation.x, local_from_grip.pose.orientation.y,
        local_from_grip.pose.orientation.z, local_from_grip.pose.orientation.w);
  }

  if (local_from_grip_speed.velocityFlags &
      XR_SPACE_VELOCITY_LINEAR_VALID_BIT) {
    pose->linear_velocity =
        gfx::Vector3dF(local_from_grip_speed.linearVelocity.x,
                       local_from_grip_speed.linearVelocity.y,
                       local_from_grip_speed.linearVelocity.z);
  }

  if (local_from_grip_speed.velocityFlags &
      XR_SPACE_VELOCITY_ANGULAR_VALID_BIT) {
    pose->angular_velocity =
        gfx::Vector3dF(local_from_grip_speed.angularVelocity.x,
                       local_from_grip_speed.angularVelocity.y,
                       local_from_grip_speed.angularVelocity.z);
  }

  return pose;
}

XrActionSet OpenXrController::GetActionSet() const {
  return action_set_;
}

XrResult OpenXrController::CreateAction(
    XrInstance instance,
    XrActionType type,
    const char* interaction_profile_name,
    const std::string& binding_string,
    const std::string& action_name,
    XrAction* action,
    std::map<XrPath, std::vector<XrActionSuggestedBinding>>* bindings) {
  XrResult xr_result;

  XrActionCreateInfo action_create_info = {XR_TYPE_ACTION_CREATE_INFO};
  action_create_info.actionType = type;

  errno_t error = strcpy_s(action_create_info.actionName, action_name.data());
  DCHECK(error == 0);
  error = strcpy_s(action_create_info.localizedActionName, action_name.data());
  DCHECK(error == 0);

  RETURN_IF_XR_FAILED(xrCreateAction(action_set_, &action_create_info, action));

  XrPath profile_path, action_path;
  RETURN_IF_XR_FAILED(
      xrStringToPath(instance, interaction_profile_name, &profile_path));
  RETURN_IF_XR_FAILED(
      xrStringToPath(instance, binding_string.c_str(), &action_path));
  (*bindings)[profile_path].push_back({*action, action_path});

  return xr_result;
}

}  // namespace device
