// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openvr/openvr_gamepad_helper.h"
#include "device/vr/openvr/openvr_api_wrapper.h"

#include <memory>
#include <unordered_set>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "device/vr/util/gamepad_builder.h"
#include "device/vr/vr_device.h"
#include "third_party/openvr/src/headers/openvr.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace device {

namespace {

// Constants/functions used by both WebXR and WebVR.
constexpr double kJoystickDeadzone = 0.1;

bool TryGetGamepadButton(const vr::VRControllerState_t& controller_state,
                         uint64_t supported_buttons,
                         vr::EVRButtonId button_id,
                         GamepadButton* button) {
  uint64_t button_mask = vr::ButtonMaskFromId(button_id);
  if ((supported_buttons & button_mask) != 0) {
    bool button_pressed = (controller_state.ulButtonPressed & button_mask) != 0;
    bool button_touched = (controller_state.ulButtonTouched & button_mask) != 0;
    button->touched = button_touched || button_pressed;
    button->pressed = button_pressed;
    button->value = button_pressed ? 1.0 : 0.0;
    return true;
  }

  return false;
}

vr::EVRButtonId GetAxisId(uint32_t axis_offset) {
  return static_cast<vr::EVRButtonId>(vr::k_EButton_Axis0 + axis_offset);
}

std::map<vr::EVRButtonId, GamepadBuilder::ButtonData> GetAxesButtons(
    vr::IVRSystem* vr_system,
    const vr::VRControllerState_t& controller_state,
    uint64_t supported_buttons,
    uint32_t controller_id) {
  std::map<vr::EVRButtonId, GamepadBuilder::ButtonData> button_data_map;

  for (uint32_t j = 0; j < vr::k_unControllerStateAxisCount; ++j) {
    int32_t axis_type = vr_system->GetInt32TrackedDeviceProperty(
        controller_id,
        static_cast<vr::TrackedDeviceProperty>(vr::Prop_Axis0Type_Int32 + j));

    GamepadBuilder::ButtonData button_data;

    // TODO(https://crbug.com/966060): Determine if inverting the y value here
    // is necessary.
    double x_axis = controller_state.rAxis[j].x;
    double y_axis = -controller_state.rAxis[j].y;

    switch (axis_type) {
      case vr::k_eControllerAxis_Joystick:
        // We only want to apply the deadzone to joysticks, since various
        // runtimes may not have already done that, but touchpads should
        // be fine.
        x_axis = std::fabs(x_axis) < kJoystickDeadzone ? 0 : x_axis;
        y_axis = std::fabs(y_axis) < kJoystickDeadzone ? 0 : y_axis;
        FALLTHROUGH;
      case vr::k_eControllerAxis_TrackPad: {
        button_data.has_both_axes = true;
        button_data.x_axis = x_axis;
        button_data.y_axis = y_axis;
        vr::EVRButtonId button_id = GetAxisId(j);

        // Even if the button associated with the axis isn't supported, if we
        // have valid axis data, we should still send that up.  Since the spec
        // expects buttons with axes, then we will add a dummy button to match
        // the axes.
        GamepadButton button;
        if (TryGetGamepadButton(controller_state, supported_buttons, button_id,
                                &button)) {
          button_data.touched = button.touched;
          button_data.pressed = button.pressed;
          button_data.value = button.value;
        } else {
          button_data.pressed = false;
          button_data.value = 0.0;
          button_data.touched =
              (std::fabs(x_axis) > 0 || std::fabs(y_axis) > 0);
        }

        button_data_map[button_id] = button_data;
      } break;
      case vr::k_eControllerAxis_Trigger: {
        GamepadButton button;
        GamepadBuilder::ButtonData button_data;
        vr::EVRButtonId button_id = GetAxisId(j);
        if (TryGetGamepadButton(controller_state, supported_buttons, button_id,
                                &button)) {
          button_data.touched = button.touched;
          button_data.pressed = button.pressed;
          button_data.value = x_axis;
          button_data_map[button_id] = button_data;
        }
      } break;
    }
  }

  return button_data_map;
}

// Constants/functions only used by WebXR.
constexpr std::array<vr::EVRButtonId, 5> kWebXRButtonOrder = {
    vr::k_EButton_A,          vr::k_EButton_DPad_Left, vr::k_EButton_DPad_Up,
    vr::k_EButton_DPad_Right, vr::k_EButton_DPad_Down,
};

// Constants/functions only used by WebVR.
constexpr std::array<vr::EVRButtonId, 7> kWebVRButtonOrder = {
    vr::k_EButton_A,
    vr::k_EButton_Grip,
    vr::k_EButton_ApplicationMenu,
    vr::k_EButton_DPad_Left,
    vr::k_EButton_DPad_Up,
    vr::k_EButton_DPad_Right,
    vr::k_EButton_DPad_Down,
};

mojom::XRGamepadButtonPtr GetMojomGamepadButton(
    const GamepadBuilder::ButtonData& data) {
  auto ret = mojom::XRGamepadButton::New();
  ret->touched = data.touched;
  ret->pressed = data.pressed;
  ret->value = data.value;

  return ret;
}

mojom::XRGamepadButtonPtr GetMojomGamepadButton(const GamepadButton& data) {
  auto ret = mojom::XRGamepadButton::New();
  ret->touched = data.touched;
  ret->pressed = data.pressed;
  ret->value = data.value;

  return ret;
}

}  // namespace

// WebVR Gamepad Getter.
mojom::XRGamepadDataPtr OpenVRGamepadHelper::GetGamepadData(
    vr::IVRSystem* vr_system) {
  mojom::XRGamepadDataPtr ret = mojom::XRGamepadData::New();

  vr::TrackedDevicePose_t tracked_devices_poses[vr::k_unMaxTrackedDeviceCount];
  vr_system->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseSeated, 0.0f,
                                             tracked_devices_poses,
                                             vr::k_unMaxTrackedDeviceCount);
  for (uint32_t i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i) {
    if (vr_system->GetTrackedDeviceClass(i) !=
        vr::TrackedDeviceClass_Controller)
      continue;

    vr::VRControllerState_t controller_state;
    bool have_state = vr_system->GetControllerState(i, &controller_state,
                                                    sizeof(controller_state));
    if (!have_state)
      continue;

    auto gamepad = mojom::XRGamepad::New();
    gamepad->controller_id = i;

    gamepad->timestamp = base::TimeTicks::Now();

    vr::ETrackedControllerRole hand =
        vr_system->GetControllerRoleForTrackedDeviceIndex(i);
    switch (hand) {
      case vr::TrackedControllerRole_Invalid:
        gamepad->hand = device::mojom::XRHandedness::NONE;
        break;
      case vr::TrackedControllerRole_LeftHand:
        gamepad->hand = device::mojom::XRHandedness::LEFT;
        break;
      case vr::TrackedControllerRole_RightHand:
        gamepad->hand = device::mojom::XRHandedness::RIGHT;
        break;
    }

    uint64_t supported_buttons = vr_system->GetUint64TrackedDeviceProperty(
        i, vr::Prop_SupportedButtons_Uint64);

    std::map<vr::EVRButtonId, GamepadBuilder::ButtonData> button_data_map =
        GetAxesButtons(vr_system, controller_state, supported_buttons, i);

    for (const auto& button_data_pair : button_data_map) {
      GamepadBuilder::ButtonData data = button_data_pair.second;

      gamepad->buttons.push_back(GetMojomGamepadButton(data));
      if (data.has_both_axes) {
        gamepad->axes.push_back(data.x_axis);
        gamepad->axes.push_back(data.y_axis);
      }
    }

    for (const auto& button : kWebVRButtonOrder) {
      GamepadButton data;
      if (TryGetGamepadButton(controller_state, supported_buttons, button,
                              &data)) {
        gamepad->buttons.push_back(GetMojomGamepadButton(data));
      }
    }

    const vr::TrackedDevicePose_t& pose = tracked_devices_poses[i];
    if (pose.bPoseIsValid) {
      const vr::HmdMatrix34_t& mat = pose.mDeviceToAbsoluteTracking;
      gfx::Transform transform(
          mat.m[0][0], mat.m[0][1], mat.m[0][2], mat.m[0][3], mat.m[1][0],
          mat.m[1][1], mat.m[1][2], mat.m[1][3], mat.m[2][0], mat.m[2][1],
          mat.m[2][2], mat.m[2][3], 0, 0, 0, 1);

      gfx::DecomposedTransform src_pose;
      gfx::DecomposeTransform(&src_pose, transform);
      auto dst_pose = mojom::VRPose::New();

      dst_pose->orientation = src_pose.quaternion;
      dst_pose->position = gfx::Point3F(
          src_pose.translate[0], src_pose.translate[1], src_pose.translate[2]);
      dst_pose->angular_velocity =
          gfx::Vector3dF(pose.vAngularVelocity.v[0], pose.vAngularVelocity.v[1],
                         pose.vAngularVelocity.v[2]);
      dst_pose->linear_velocity = gfx::Vector3dF(
          pose.vVelocity.v[0], pose.vVelocity.v[1], pose.vVelocity.v[2]);

      gamepad->pose = std::move(dst_pose);
    }

    ret->gamepads.push_back(std::move(gamepad));
  }

  return ret;
}

// Helper classes and WebXR Getters
class OpenVRGamepadBuilder : public GamepadBuilder {
 public:
  enum class AxesRequirement {
    kOptional = 0,
    kRequireBoth = 1,
  };

  OpenVRGamepadBuilder(vr::IVRSystem* vr_system,
                       uint32_t controller_id,
                       vr::VRControllerState_t controller_state,
                       device::mojom::XRHandedness handedness)
      : GamepadBuilder(GetGamepadId(vr_system, controller_id),
                       GamepadMapping::kXrStandard,
                       handedness),
        controller_state_(controller_state) {
    supported_buttons_ = vr_system->GetUint64TrackedDeviceProperty(
        controller_id, vr::Prop_SupportedButtons_Uint64);

    axes_data_ = GetAxesButtons(vr_system, controller_state_,
                                supported_buttons_, controller_id);
  }

  ~OpenVRGamepadBuilder() override = default;

  bool TryAddAxesOrTriggerButton(
      vr::EVRButtonId button_id,
      AxesRequirement requirement = AxesRequirement::kOptional) {
    if (!IsInAxesData(button_id))
      return false;

    bool require_axes = (requirement == AxesRequirement::kRequireBoth);
    if (require_axes && !axes_data_[button_id].has_both_axes)
      return false;

    AddButton(axes_data_[button_id]);
    used_axes_.insert(button_id);

    return true;
  }

  bool TryAddNextUnusedButtonWithAxes() {
    for (const auto& axes_data_pair : axes_data_) {
      vr::EVRButtonId button_id = axes_data_pair.first;
      if (IsUsed(button_id))
        continue;

      if (TryAddAxesOrTriggerButton(button_id, AxesRequirement::kRequireBoth))
        return true;
    }

    return false;
  }

  bool TryAddButton(vr::EVRButtonId button_id) {
    GamepadButton button;
    if (TryGetGamepadButton(controller_state_, supported_buttons_, button_id,
                            &button)) {
      AddButton(button);
      return true;
    }

    return false;
  }

  // This will add any remaining unused values from axes_data to the gamepad.
  // Returns a bool indicating whether any additional axes were added.
  bool AddRemainingTriggersAndAxes() {
    bool added_axes = false;
    for (const auto& axes_data_pair : axes_data_) {
      if (!IsUsed(axes_data_pair.first)) {
        added_axes = true;
        AddButton(axes_data_pair.second);
      }
    }

    return added_axes;
  }

 private:
  static bool IsControllerHTCVive(vr::IVRSystem* vr_system,
                                  uint32_t controller_id) {
    std::string model =
        GetOpenVRString(vr_system, vr::Prop_ModelNumber_String, controller_id);
    std::string manufacturer = GetOpenVRString(
        vr_system, vr::Prop_ManufacturerName_String, controller_id);

    // OpenVR reports different model strings for developer vs released versions
    // of Vive controllers. In the future, there could be additional iterations
    // of the Vive controller that we also want to catch here. That's why we
    // check if the model string contains "Vive" instead of doing an exact match
    // against specific string(s).
    return (manufacturer == "HTC") && (model.find("Vive") != std::string::npos);
  }

  // TODO(https://crbug.com/942201): Get correct ID string once WebXR spec issue
  // #550 (https://github.com/immersive-web/webxr/issues/550) is resolved.
  static std::string GetGamepadId(vr::IVRSystem* vr_system,
                                  uint32_t controller_id) {
    if (IsControllerHTCVive(vr_system, controller_id)) {
      return "htc-vive";
    }

    return "openvr";
  }

  bool IsUsed(vr::EVRButtonId button_id) {
    auto it = used_axes_.find(button_id);
    return it != used_axes_.end();
  }

  bool IsInAxesData(vr::EVRButtonId button_id) {
    auto it = axes_data_.find(button_id);
    return it != axes_data_.end();
  }

  const vr::VRControllerState_t controller_state_;
  uint64_t supported_buttons_;
  std::map<vr::EVRButtonId, ButtonData> axes_data_;
  std::unordered_set<vr::EVRButtonId> used_axes_;

  DISALLOW_COPY_AND_ASSIGN(OpenVRGamepadBuilder);
};

base::Optional<Gamepad> OpenVRGamepadHelper::GetXRGamepad(
    vr::IVRSystem* vr_system,
    uint32_t controller_id,
    vr::VRControllerState_t controller_state,
    device::mojom::XRHandedness handedness) {
  OpenVRGamepadBuilder builder(vr_system, controller_id, controller_state,
                               handedness);

  if (!builder.TryAddAxesOrTriggerButton(vr::k_EButton_SteamVR_Trigger))
    return base::nullopt;

  if (!builder.TryAddNextUnusedButtonWithAxes())
    return base::nullopt;

  bool added_placeholder_grip = false;
  if (!builder.TryAddButton(vr::k_EButton_Grip)) {
    added_placeholder_grip = true;
    builder.AddPlaceholderButton();
  }

  // If we can't find any secondary button with an x and y axis, add a fake
  // button.  Note that we're not worried about ensuring that the axes data gets
  // added, because if there were any other axes to add, we would've added them.
  bool added_placeholder_axes = false;
  if (!builder.TryAddNextUnusedButtonWithAxes()) {
    added_placeholder_axes = true;
    builder.AddPlaceholderButton();
  }

  // Now that all of the xr-standard reserved buttons have been filled in, we
  // add the rest of the buttons in order of decreasing importance.
  // First add regular buttons
  bool added_optional_buttons = false;
  for (const auto& button : kWebXRButtonOrder) {
    added_optional_buttons =
        builder.TryAddButton(button) || added_optional_buttons;
  }

  // Finally, add any remaining axis buttons (triggers/josysticks/touchpads)
  bool added_optional_axes = builder.AddRemainingTriggersAndAxes();

  // If we didn't add any optional buttons, we need to remove our placeholder
  // buttons.
  if (!(added_optional_buttons || added_optional_axes)) {
    // If we didn't add any optional buttons, see if we need to remove the most
    // recent placeholder (the secondary axes).
    // Note that if we added a placeholder axes, the only optional axes that
    // should have been added are triggers, and so we don't need to worry about
    // the order
    if (added_placeholder_axes) {
      builder.RemovePlaceholderButton();

      // Only if the axes button was a placeholder can we remove the grip
      // if it was also a placeholder.
      if (added_placeholder_grip) {
        builder.RemovePlaceholderButton();
      }
    }
  }

  return builder.GetGamepad();
}

}  // namespace device
