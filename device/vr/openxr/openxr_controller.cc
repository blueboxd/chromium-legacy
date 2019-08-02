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
      action_set_(XR_NULL_HANDLE),
      palm_pose_action_{XR_NULL_HANDLE},
      palm_pose_space_(XR_NULL_HANDLE) {}

OpenXrController::~OpenXrController() {
  // We don't need to destroy all of the actions because destroying an
  // action set destroys all actions that are part of the set.

  if (action_set_ != XR_NULL_HANDLE) {
    xrDestroyActionSet(action_set_);
  }
  if (palm_pose_space_ != XR_NULL_HANDLE) {
    xrDestroySpace(palm_pose_space_);
  }
}

XrResult OpenXrController::Initialize(
    OpenXrControllerType type,
    XrInstance instance,
    XrSession session,
    std::map<XrPath, std::vector<XrActionSuggestedBinding>>* bindings) {
  type_ = type;
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
      xrCreateActionSet(session, &action_set_create_info, &action_set_));

  RETURN_IF_XR_FAILED(InitializeMicrosoftMotionControllers(
      instance, session, type_string, bindings));

  XrActionSpaceCreateInfo action_space_create_info = {
      XR_TYPE_ACTION_SPACE_CREATE_INFO};
  action_space_create_info.poseInActionSpace = PoseIdentity();
  RETURN_IF_XR_FAILED(xrCreateActionSpace(
      palm_pose_action_, &action_space_create_info, &palm_pose_space_));
  return xr_result;
}

XrResult OpenXrController::InitializeMicrosoftMotionControllers(
    XrInstance instance,
    XrSession session,
    const std::string& type_string,
    std::map<XrPath, std::vector<XrActionSuggestedBinding>>* bindings) {
  XrResult xr_result;

  const std::string binding_prefix = "/user/hand/" + type_string + "/input/";
  const std::string name_prefix = type_string + "_controller_";

  RETURN_IF_XR_FAILED(CreateAction(
      instance, XR_INPUT_ACTION_TYPE_BOOLEAN, kMicrosoftInteractionProfileName,
      binding_prefix + "trackpad/click", name_prefix + "trackpad_button_press",
      &(button_action_map_[OpenXrButtonType::kTrackpad].press_action),
      bindings));

  RETURN_IF_XR_FAILED(CreateAction(
      instance, XR_INPUT_ACTION_TYPE_BOOLEAN, kMicrosoftInteractionProfileName,
      binding_prefix + "trigger/value", name_prefix + "trigger_button_press",
      &(button_action_map_[OpenXrButtonType::kTrigger].press_action),
      bindings));

  RETURN_IF_XR_FAILED(CreateAction(
      instance, XR_INPUT_ACTION_TYPE_BOOLEAN, kMicrosoftInteractionProfileName,
      binding_prefix + "grip/click", name_prefix + "grip_button_press",
      &(button_action_map_[OpenXrButtonType::kGrip].press_action), bindings));

  RETURN_IF_XR_FAILED(CreateAction(
      instance, XR_INPUT_ACTION_TYPE_BOOLEAN, kMicrosoftInteractionProfileName,
      binding_prefix + "menu/click", name_prefix + "menu_button_press",
      &(button_action_map_[OpenXrButtonType::kMenu].press_action), bindings));

  RETURN_IF_XR_FAILED(CreateAction(
      instance, XR_INPUT_ACTION_TYPE_VECTOR2F, kMicrosoftInteractionProfileName,
      binding_prefix + "trackpad", name_prefix + "trackpad_axis",
      &(axis_action_map_[OpenXrAxisType::kTrackpad]), bindings));

  RETURN_IF_XR_FAILED(CreateAction(
      instance, XR_INPUT_ACTION_TYPE_VECTOR2F, kMicrosoftInteractionProfileName,
      binding_prefix + "thumbstick", name_prefix + "thumbstick_axis",
      &(axis_action_map_[OpenXrAxisType::kThumbstick]), bindings));

  RETURN_IF_XR_FAILED(
      CreateAction(instance, XR_INPUT_ACTION_TYPE_POSE,
                   kMicrosoftInteractionProfileName, binding_prefix + "palm",
                   name_prefix + "grip_pose", &palm_pose_action_, bindings));

  return xr_result;
}

int OpenXrController::GetID() {
  return static_cast<int>(type_);
}

device::mojom::XRHandedness OpenXrController::GetHandness() {
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

XrResult OpenXrController::GetButton(OpenXrButtonType type,
                                     mojom::XRGamepadButtonPtr* data) {
  XrResult xr_result;
  XrActionStateBoolean press_state_bool = {XR_TYPE_ACTION_STATE_BOOLEAN};
  RETURN_IF_XR_FAILED(
      QueryState(button_action_map_[type].press_action, &press_state_bool));
  RETURN_IF_XR_STATE_IS_NOT_ACTIVE(press_state_bool);
  mojom::XRGamepadButtonPtr mojo_button_ptr =
      GetGamepadButton(press_state_bool);
  *data = std::move(mojo_button_ptr);
  return xr_result;
}

XrResult OpenXrController::GetWebVrButtons(
    std::vector<mojom::XRGamepadButtonPtr>* buttons) {
  DCHECK(buttons);
  XrResult xr_result;

  for (uint32_t i = 0; i <= static_cast<uint32_t>(OpenXrButtonType::kMaxValue);
       i++) {
    mojom::XRGamepadButtonPtr mojo_button_ptr;
    RETURN_IF_XR_FAILED(
        GetButton(static_cast<OpenXrButtonType>(i), &mojo_button_ptr));
    if (xr_result == XR_STATE_UNAVAILABLE) {
      return xr_result;
    }
    buttons->push_back(std::move(mojo_button_ptr));
  }

  return xr_result;
}

XrResult OpenXrController::GetAxes(OpenXrAxisType type,
                                   std::array<double, 2>* axes) {
  DCHECK(axes);
  XrResult xr_result;

  XrActionStateVector2f axis_state_v2f = {XR_TYPE_ACTION_STATE_VECTOR2F};
  RETURN_IF_XR_FAILED(QueryState(axis_action_map_[type], &axis_state_v2f));
  RETURN_IF_XR_STATE_IS_NOT_ACTIVE(axis_state_v2f);
  (*axes)[0] = axis_state_v2f.currentState.x;
  (*axes)[1] = axis_state_v2f.currentState.y;

  return xr_result;
}

XrResult OpenXrController::GetAllWebVrAxes(std::vector<double>* axes) {
  DCHECK(axes);
  XrResult xr_result;

  for (uint32_t i = 0; i <= static_cast<uint32_t>(OpenXrAxisType::kMaxValue);
       i++) {
    std::array<double, 2> axe;
    RETURN_IF_XR_FAILED(GetAxes(static_cast<OpenXrAxisType>(i), &axe));
    if (xr_result == XR_STATE_UNAVAILABLE) {
      return xr_result;
    }
    axes->insert(axes->end(), axe.begin(), axe.end());
  }
  return xr_result;
}

XrResult OpenXrController::GetPose(XrTime predicted_display_time,
                                   XrSpace local_space,
                                   mojom::XRGamepad* gamepad_ptr) {
  XrResult xr_result;

  XrActionStatePose palm_state_pose = {XR_TYPE_ACTION_STATE_POSE};
  RETURN_IF_XR_FAILED(QueryState(palm_pose_action_, &palm_state_pose));
  RETURN_IF_XR_STATE_IS_NOT_ACTIVE(palm_state_pose);

  XrSpaceRelation space_relation = {XR_TYPE_SPACE_RELATION};
  RETURN_IF_XR_FAILED(xrLocateSpace(palm_pose_space_, local_space,
                                    predicted_display_time, &space_relation));

  gamepad_ptr->pose = mojom::VRPose::New();

  if (space_relation.relationFlags & XR_SPACE_RELATION_POSITION_VALID_BIT) {
    gamepad_ptr->can_provide_position = true;
    gamepad_ptr->pose->position = gfx::Point3F(space_relation.pose.position.x,
                                               space_relation.pose.position.y,
                                               space_relation.pose.position.z);
  }

  if (space_relation.relationFlags & XR_SPACE_RELATION_ORIENTATION_VALID_BIT) {
    gamepad_ptr->can_provide_orientation = true;
    gamepad_ptr->pose->orientation = gfx::Quaternion(
        space_relation.pose.orientation.x, space_relation.pose.orientation.y,
        space_relation.pose.orientation.z, space_relation.pose.orientation.w);
  }

  if (space_relation.relationFlags &
      XR_SPACE_RELATION_LINEAR_VELOCITY_VALID_BIT) {
    gamepad_ptr->pose->linear_velocity = gfx::Vector3dF(
        space_relation.linearVelocity.x, space_relation.linearVelocity.y,
        space_relation.linearVelocity.z);
  }

  if (space_relation.relationFlags &
      XR_SPACE_RELATION_LINEAR_ACCELERATION_VALID_BIT) {
    gamepad_ptr->pose->linear_acceleration =
        gfx::Vector3dF(space_relation.linearAcceleration.x,
                       space_relation.linearAcceleration.y,
                       space_relation.linearAcceleration.z);
  }

  if (space_relation.relationFlags &
      XR_SPACE_RELATION_ANGULAR_VELOCITY_VALID_BIT) {
    gamepad_ptr->pose->angular_velocity = gfx::Vector3dF(
        space_relation.angularVelocity.x, space_relation.angularVelocity.y,
        space_relation.angularVelocity.z);
  }

  if (space_relation.relationFlags &
      XR_SPACE_RELATION_ANGULAR_ACCELERATION_VALID_BIT) {
    gamepad_ptr->pose->angular_acceleration =
        gfx::Vector3dF(space_relation.angularAcceleration.x,
                       space_relation.angularAcceleration.y,
                       space_relation.angularAcceleration.z);
  }

  return xr_result;
}

mojom::XRGamepadButtonPtr OpenXrController::GetGamepadButton(
    const XrActionStateBoolean& action_state) {
  mojom::XRGamepadButtonPtr ret = mojom::XRGamepadButton::New();
  bool button_pressed = action_state.currentState;
  ret->touched = button_pressed;
  ret->pressed = button_pressed;
  ret->value = button_pressed ? 1.0 : 0.0;

  return ret;
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
