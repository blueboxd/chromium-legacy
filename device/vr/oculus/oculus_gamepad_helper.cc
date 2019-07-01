// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/oculus/oculus_gamepad_helper.h"

#include <algorithm>
#include <memory>

#include "base/logging.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "device/vr/util/gamepad_builder.h"
#include "device/vr/vr_device.h"
#include "third_party/libovr/src/Include/OVR_CAPI.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace device {

namespace {

// TODO(https://crbug.com/953489): This is currently WebVR-only. If it is useful
// for addressing this issue, give it a more meaningful name. Otherwise, remove
// this enum when WebVR support is removed.
enum GamepadIndex : unsigned int {
  kLeftTouchController = 0x0,
  kRightTouchController = 0x1,
  kRemote = 0x2,
};

float ApplyTriggerDeadzone(float value) {
  // Trigger value should be between 0 and 1.  We apply a deadzone for small
  // values so a loose controller still reports a value of 0 when not in use.
  float kTriggerDeadzone = 0.01f;

  return (value < kTriggerDeadzone) ? 0 : value;
}

mojom::XRGamepadButtonPtr GetGamepadButton(const ovrInputState& input_state,
                                           ovrButton button_id) {
  bool pressed = (input_state.Buttons & button_id) != 0;
  bool touched = (input_state.Touches & button_id) != 0;

  auto button = mojom::XRGamepadButton::New();
  button->pressed = pressed;
  button->touched = touched;
  button->value = pressed ? 1.0f : 0.0f;

  return button;
}

mojom::XRGamepadButtonPtr GetGamepadTouchTrigger(
    const ovrInputState& input_state,
    ovrTouch touch_id,
    float value) {
  bool touched = (input_state.Touches & touch_id) != 0;
  value = ApplyTriggerDeadzone(value);

  auto button = mojom::XRGamepadButton::New();
  button->pressed = value != 0;
  button->touched = touched;
  button->value = value;

  return button;
}

mojom::XRGamepadButtonPtr GetGamepadTrigger(float value) {
  value = ApplyTriggerDeadzone(value);
  auto button = mojom::XRGamepadButton::New();
  button->pressed = value != 0;
  button->touched = value != 0;
  button->value = value;

  return button;
}

mojom::XRGamepadButtonPtr GetGamepadTouch(const ovrInputState& input_state,
                                          ovrTouch touch_id) {
  bool touched = (input_state.Touches & touch_id) != 0;

  auto button = mojom::XRGamepadButton::New();
  button->pressed = false;
  button->touched = touched;
  button->value = 0.0f;

  return button;
}

void AddTouchData(mojom::XRGamepadDataPtr& data,
                  const ovrTrackingState& tracking,
                  const ovrInputState& input_state,
                  ovrHandType hand) {
  auto gamepad = mojom::XRGamepad::New();
  // This gamepad layout is the defacto standard, but can be adjusted for WebXR.
  switch (hand) {
    case ovrHand_Left:
      gamepad->hand = device::mojom::XRHandedness::LEFT;
      gamepad->controller_id = kLeftTouchController;
      gamepad->buttons.push_back(
          GetGamepadButton(input_state, ovrButton_LThumb));
      break;
    case ovrHand_Right:
      gamepad->hand = device::mojom::XRHandedness::RIGHT;
      gamepad->controller_id = kRightTouchController;
      gamepad->buttons.push_back(
          GetGamepadButton(input_state, ovrButton_RThumb));
      break;
    default:
      NOTREACHED();
      return;
  }

  gamepad->axes.push_back(input_state.Thumbstick[hand].x);
  gamepad->axes.push_back(-input_state.Thumbstick[hand].y);

  gamepad->buttons.push_back(GetGamepadTouchTrigger(
      input_state,
      hand == ovrHand_Left ? ovrTouch_LIndexTrigger : ovrTouch_RIndexTrigger,
      input_state.IndexTrigger[hand]));
  gamepad->buttons.push_back(GetGamepadTrigger(input_state.HandTrigger[hand]));

  switch (hand) {
    case ovrHand_Left:
      gamepad->buttons.push_back(GetGamepadButton(input_state, ovrButton_X));
      gamepad->buttons.push_back(GetGamepadButton(input_state, ovrButton_Y));
      gamepad->buttons.push_back(
          GetGamepadTouch(input_state, ovrTouch_LThumbRest));
      break;
    case ovrHand_Right:
      gamepad->buttons.push_back(GetGamepadButton(input_state, ovrButton_A));
      gamepad->buttons.push_back(GetGamepadButton(input_state, ovrButton_B));
      gamepad->buttons.push_back(
          GetGamepadTouch(input_state, ovrTouch_RThumbRest));
      break;
    default:
      NOTREACHED();
      break;
  }

  auto dst_pose = mojom::VRPose::New();
  const ovrPoseStatef& src_pose = tracking.HandPoses[hand];

  dst_pose->orientation = gfx::Quaternion(
      src_pose.ThePose.Orientation.x, src_pose.ThePose.Orientation.y,
      src_pose.ThePose.Orientation.z, src_pose.ThePose.Orientation.w);

  dst_pose->position =
      gfx::Point3F(src_pose.ThePose.Position.x, src_pose.ThePose.Position.y,
                   src_pose.ThePose.Position.z);

  dst_pose->angular_velocity =
      gfx::Vector3dF(src_pose.AngularVelocity.x, src_pose.AngularVelocity.y,
                     src_pose.AngularVelocity.z);

  dst_pose->linear_velocity =
      gfx::Vector3dF(src_pose.LinearVelocity.x, src_pose.LinearVelocity.y,
                     src_pose.LinearVelocity.z);

  dst_pose->angular_acceleration = gfx::Vector3dF(
      src_pose.AngularAcceleration.x, src_pose.AngularAcceleration.y,
      src_pose.AngularAcceleration.z);

  dst_pose->linear_acceleration = gfx::Vector3dF(src_pose.LinearAcceleration.x,
                                                 src_pose.LinearAcceleration.y,
                                                 src_pose.LinearAcceleration.z);

  gamepad->pose = std::move(dst_pose);
  gamepad->can_provide_position = true;
  gamepad->can_provide_orientation = true;
  gamepad->timestamp = base::TimeTicks::Now();
  data->gamepads.push_back(std::move(gamepad));
}

void AddRemoteData(mojom::XRGamepadDataPtr& data,
                   const ovrInputState& input_remote) {
  auto remote = mojom::XRGamepad::New();
  remote->buttons.push_back(GetGamepadButton(input_remote, ovrButton_Enter));
  remote->buttons.push_back(GetGamepadButton(input_remote, ovrButton_Back));
  remote->buttons.push_back(GetGamepadButton(input_remote, ovrButton_Up));
  remote->buttons.push_back(GetGamepadButton(input_remote, ovrButton_Down));
  remote->buttons.push_back(GetGamepadButton(input_remote, ovrButton_Left));
  remote->buttons.push_back(GetGamepadButton(input_remote, ovrButton_Right));
  remote->controller_id = kRemote;
  remote->hand = device::mojom::XRHandedness::NONE;
  data->gamepads.push_back(std::move(remote));
}

device::mojom::XRHandedness OculusToMojomHand(ovrHandType hand) {
  switch (hand) {
    case ovrHand_Left:
      return device::mojom::XRHandedness::LEFT;
    case ovrHand_Right:
      return device::mojom::XRHandedness::RIGHT;
    default:
      return device::mojom::XRHandedness::NONE;
  }
}

class OculusGamepadBuilder : public GamepadBuilder {
 public:
  // TODO(https://crbug.com/942201): Get correct ID string once WebXR spec issue
  // #550 (https://github.com/immersive-web/webxr/issues/550) is resolved.
  OculusGamepadBuilder(ovrInputState state, ovrHandType hand)
      : GamepadBuilder("oculus-touch",
                       GamepadMapping::kXrStandard,
                       OculusToMojomHand(hand)),
        state_(state) {}

  ~OculusGamepadBuilder() override = default;

  void AddStandardButton(ovrButton id) {
    bool pressed = (state_.Buttons & id) != 0;
    bool touched = (state_.Touches & id) != 0;
    double value = pressed ? 1.0 : 0.0;
    AddButton(GamepadButton(pressed, touched, value));
  }

  void AddTouchButton(ovrTouch id) {
    bool touched = (state_.Touches & id) != 0;
    AddButton(GamepadButton(false, touched, 0.0f));
  }

  void AddTriggerButton(float value) {
    value = ApplyTriggerDeadzone(value);
    bool pressed = value != 0;
    bool touched = pressed;
    AddButton(GamepadButton(pressed, touched, value));
  }

  void AddTouchTriggerButton(ovrTouch id, float value) {
    value = ApplyTriggerDeadzone(value);
    bool pressed = value != 0;
    bool touched = (state_.Touches & id) != 0;
    AddButton(GamepadButton(pressed, touched, value));
  }

  bool IsValid() const override {
    return GamepadBuilder::IsValid() && GetHandedness() != GamepadHand::kNone;
  }

 private:
  ovrInputState state_;

  DISALLOW_COPY_AND_ASSIGN(OculusGamepadBuilder);
};

}  // namespace

mojom::XRGamepadDataPtr OculusGamepadHelper::GetGamepadData(
    ovrSession session) {
  ovrInputState input_touch;
  bool have_touch = OVR_SUCCESS(
      ovr_GetInputState(session, ovrControllerType_Touch, &input_touch));

  ovrInputState input_remote;
  bool have_remote = OVR_SUCCESS(
      ovr_GetInputState(session, ovrControllerType_Remote, &input_remote));

  ovrTrackingState tracking = ovr_GetTrackingState(session, 0, false);

  mojom::XRGamepadDataPtr data = mojom::XRGamepadData::New();

  if (have_touch) {
    AddTouchData(data, tracking, input_touch, ovrHand_Left);
    AddTouchData(data, tracking, input_touch, ovrHand_Right);
  }

  if (have_remote)
    AddRemoteData(data, input_remote);

  return data;
}

// Order of buttons 1-4 is dictated by the xr-standard Gamepad mapping.
// Buttons 5-7 are in order of decreasing importance.
// 1) index trigger (primary trigger/button)
// 2) thumb joystick button
// 3) hand trigger (primary trigger/button)
// 4) EMPTY (no secondary joystick/touchpad exists)
// 5) A or X
// 6) B or Y
// 7) thumbrest touch sensor
base::Optional<Gamepad> OculusGamepadHelper::CreateGamepad(ovrSession session,
                                                           ovrHandType hand) {
  ovrInputState input_touch;
  bool have_touch = OVR_SUCCESS(
      ovr_GetInputState(session, ovrControllerType_Touch, &input_touch));
  if (!have_touch) {
    return base::nullopt;
  }

  OculusGamepadBuilder touch(input_touch, hand);

  // TODO(https://crbug.com/966060): Determine if inverting the y value here is
  // necessary.
  touch.AddAxis(input_touch.Thumbstick[hand].x);
  touch.AddAxis(-input_touch.Thumbstick[hand].y);

  switch (hand) {
    case ovrHand_Left:
      touch.AddTouchTriggerButton(ovrTouch_LIndexTrigger,
                                  input_touch.IndexTrigger[hand]);
      touch.AddStandardButton(ovrButton_LThumb);
      touch.AddTriggerButton(input_touch.HandTrigger[hand]);
      touch.AddPlaceholderButton();
      touch.AddStandardButton(ovrButton_X);
      touch.AddStandardButton(ovrButton_Y);
      touch.AddTouchButton(ovrTouch_LThumbRest);
      break;
    case ovrHand_Right:
      touch.AddTouchTriggerButton(ovrTouch_RIndexTrigger,
                                  input_touch.IndexTrigger[hand]);
      touch.AddStandardButton(ovrButton_RThumb);
      touch.AddTriggerButton(input_touch.HandTrigger[hand]);
      touch.AddPlaceholderButton();
      touch.AddStandardButton(ovrButton_A);
      touch.AddStandardButton(ovrButton_B);
      touch.AddTouchButton(ovrTouch_RThumbRest);
      break;
    default:
      DLOG(WARNING) << "Unsupported hand configuration.";
      return base::nullopt;
  }

  return touch.GetGamepad();
}

}  // namespace device
