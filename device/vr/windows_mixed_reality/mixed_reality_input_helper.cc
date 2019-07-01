// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/mixed_reality_input_helper.h"

#include <windows.perception.spatial.h>
#include <windows.ui.input.spatial.h>

#include <wrl.h>
#include <wrl/event.h>

#include <unordered_map>
#include <vector>

#include "base/strings/safe_sprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/util/gamepad_builder.h"
#include "device/vr/windows_mixed_reality/mixed_reality_renderloop.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_input_location.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_input_manager.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_input_source.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_input_source_state.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_origins.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_pointer_pose.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_pointer_source_pose.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_timestamp.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_wrapper_factories.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace device {

// We want to differentiate from gfx::Members, so we're not going to explicitly
// use anything from Windows::Foundation::Numerics
namespace WFN = ABI::Windows::Foundation::Numerics;

using Handedness =
    ABI::Windows::UI::Input::Spatial::SpatialInteractionSourceHandedness;
using PressKind = ABI::Windows::UI::Input::Spatial::SpatialInteractionPressKind;
using SourceKind =
    ABI::Windows::UI::Input::Spatial::SpatialInteractionSourceKind;

ParsedInputState::ParsedInputState() = default;
ParsedInputState::~ParsedInputState() = default;
ParsedInputState::ParsedInputState(ParsedInputState&& other) = default;

MixedRealityInputHelper::ControllerState::ControllerState() = default;
MixedRealityInputHelper::ControllerState::~ControllerState() = default;

namespace {

// Helpers for WebVR Gamepad
constexpr double kDeadzoneMinimum = 0.1;

double ApplyAxisDeadzone(double value) {
  return std::fabs(value) < kDeadzoneMinimum ? 0 : value;
}

void AddButton(mojom::XRGamepadPtr& gamepad,
               const GamepadBuilder::ButtonData* data) {
  if (data) {
    auto button = mojom::XRGamepadButton::New();
    button->pressed = data->pressed;
    button->touched = data->touched;
    button->value = data->value;

    gamepad->buttons.push_back(std::move(button));
  } else {
    gamepad->buttons.push_back(mojom::XRGamepadButton::New());
  }
}

// These methods are only called for the thumbstick and touchpad, which both
// have an X and Y.
void AddAxes(mojom::XRGamepadPtr& gamepad,
             const GamepadBuilder::ButtonData& data) {
  gamepad->axes.push_back(ApplyAxisDeadzone(data.x_axis));
  gamepad->axes.push_back(ApplyAxisDeadzone(data.y_axis));
}

void AddButtonWithAxes(mojom::XRGamepadPtr& gamepad,
                       const GamepadBuilder::ButtonData& data) {
  AddButton(gamepad, &data);
  AddAxes(gamepad, data);
}

gfx::Point3F ConvertToPoint3F(const GamepadVector& vector) {
  return gfx::Point3F(vector.x, vector.y, vector.z);
}

gfx::Vector3dF ConvertToVector3dF(const GamepadVector& vector) {
  return gfx::Vector3dF(vector.x, vector.y, vector.z);
}

gfx::Quaternion ConvertToQuaternion(const GamepadQuaternion& quat) {
  return gfx::Quaternion(quat.x, quat.y, quat.z, quat.w);
}

mojom::VRPosePtr ConvertToVRPose(const GamepadPose& gamepad_pose) {
  if (!gamepad_pose.not_null)
    return nullptr;

  auto pose = mojom::VRPose::New();
  if (gamepad_pose.orientation.not_null)
    pose->orientation = ConvertToQuaternion(gamepad_pose.orientation);

  if (gamepad_pose.position.not_null)
    pose->position = ConvertToPoint3F(gamepad_pose.position);

  if (gamepad_pose.angular_velocity.not_null)
    pose->angular_velocity = ConvertToVector3dF(gamepad_pose.angular_velocity);

  if (gamepad_pose.linear_velocity.not_null)
    pose->linear_velocity = ConvertToVector3dF(gamepad_pose.linear_velocity);

  if (gamepad_pose.angular_acceleration.not_null)
    pose->angular_acceleration =
        ConvertToVector3dF(gamepad_pose.angular_acceleration);

  if (gamepad_pose.linear_acceleration.not_null)
    pose->linear_acceleration =
        ConvertToVector3dF(gamepad_pose.linear_acceleration);

  return pose;
}

GamepadQuaternion ConvertToGamepadQuaternion(const WFN::Quaternion& quat) {
  GamepadQuaternion gamepad_quaternion;
  gamepad_quaternion.not_null = true;
  gamepad_quaternion.x = quat.X;
  gamepad_quaternion.y = quat.Y;
  gamepad_quaternion.z = quat.Z;
  gamepad_quaternion.w = quat.W;
  return gamepad_quaternion;
}

GamepadVector ConvertToGamepadVector(const WFN::Vector3& vec3) {
  GamepadVector gamepad_vector;
  gamepad_vector.not_null = true;
  gamepad_vector.x = vec3.X;
  gamepad_vector.y = vec3.Y;
  gamepad_vector.z = vec3.Z;
  return gamepad_vector;
}

GamepadPose GetGamepadPose(const WMRInputLocation* location) {
  GamepadPose gamepad_pose;

  WFN::Quaternion quat;
  if (location->TryGetOrientation(&quat)) {
    gamepad_pose.not_null = true;
    gamepad_pose.orientation = ConvertToGamepadQuaternion(quat);
  }

  WFN::Vector3 vec3;
  if (location->TryGetPosition(&vec3)) {
    gamepad_pose.not_null = true;
    gamepad_pose.position = ConvertToGamepadVector(vec3);
  }

  if (location->TryGetVelocity(&vec3)) {
    gamepad_pose.not_null = true;
    gamepad_pose.linear_velocity = ConvertToGamepadVector(vec3);
  }

  if (location->TryGetAngularVelocity(&vec3)) {
    gamepad_pose.not_null = true;
    gamepad_pose.angular_velocity = ConvertToGamepadVector(vec3);
  }

  return gamepad_pose;
}

mojom::XRGamepadPtr GetWebVRGamepad(ParsedInputState input_state) {
  auto gamepad = mojom::XRGamepad::New();
  // This matches the order of button trigger events from Edge.  Note that we
  // use the polled button state for select here.  Voice (which we cannot get
  // via polling), lacks enough data to be considered a "Gamepad", and if we
  // used eventing the pressed state may be inconsistent.
  AddButtonWithAxes(gamepad, input_state.button_data[ButtonName::kThumbstick]);
  AddButton(gamepad, &input_state.button_data[ButtonName::kSelect]);
  AddButton(gamepad, &input_state.button_data[ButtonName::kGrip]);
  AddButton(gamepad, nullptr);  // Nothing seems to trigger this button in Edge.
  AddButtonWithAxes(gamepad, input_state.button_data[ButtonName::kTouchpad]);

  auto handedness = input_state.source_state->description->handedness;

  gamepad->timestamp = base::TimeTicks::Now();
  gamepad->hand = handedness;
  gamepad->controller_id = input_state.source_state->source_id;

  // We need to ensure that we have a VRPose so that we can attach input_states
  // and therefore a gamepad to plumb up the Gamepad Id with VendorId/ProductId.
  if (input_state.gamepad_pose.not_null) {
    gamepad->pose = ConvertToVRPose(input_state.gamepad_pose);
    gamepad->can_provide_position = input_state.gamepad_pose.position.not_null;
    gamepad->can_provide_orientation =
        input_state.gamepad_pose.orientation.not_null;
  } else {
    gamepad->can_provide_orientation = false;
    gamepad->can_provide_position = false;
    gamepad->pose = mojom::VRPose::New();
  }

  // Build the gamepad id, this prefix is used for all controller types and
  // VendorId-ProductId is appended after it, padded with leading 0's.
  char gamepad_id[Gamepad::kIdLengthCap];
  base::strings::SafeSPrintf(
      gamepad_id, "Spatial Controller (Spatial Interaction Source) %04X-%04X",
      input_state.vendor_id, input_state.product_id);

  // We have to use the GamepadBuilder because the mojom serialization complains
  // if some of the values are missing/invalid.
  GamepadBuilder builder(gamepad_id, GamepadMapping::kNone, handedness);

  auto input_source_state = mojom::XRInputSourceState::New();
  input_source_state->gamepad = builder.GetGamepad();

  // Typical chromium style would be to use the initializer list, but that
  // doesn't seem to be compatible with the explicitly deleted move/copy
  // constructors for the vector.
  std::vector<mojom::XRInputSourceStatePtr> input_source_vector;
  input_source_vector.push_back(std::move(input_source_state));
  gamepad->pose->input_state = std::move(input_source_vector);

  return gamepad;
}

// Helpers for WebXRGamepad
base::Optional<Gamepad> GetWebXRGamepad(ParsedInputState& input_state) {
  device::mojom::XRHandedness handedness = device::mojom::XRHandedness::NONE;
  if (input_state.source_state && input_state.source_state->description)
    handedness = input_state.source_state->description->handedness;

  // TODO(https://crbug.com/942201): Get correct ID string once WebXR spec issue
  // #550 (https://github.com/immersive-web/webxr/issues/550) is resolved.
  GamepadBuilder builder("windows-mixed-reality", GamepadMapping::kXrStandard,
                         handedness);

  builder.SetAxisDeadzone(kDeadzoneMinimum);

  // The order of these buttons is dictated by the xr-standard Gamepad mapping.
  // Thumbstick is considered the primary 2D input axis, while the touchpad is
  // the secondary 2D input axis.  If any of these are missing, map will give
  // us a default version, which is fine.
  builder.AddButton(input_state.button_data[ButtonName::kSelect]);
  builder.AddButton(input_state.button_data[ButtonName::kThumbstick]);
  builder.AddButton(input_state.button_data[ButtonName::kGrip]);
  builder.AddButton(input_state.button_data[ButtonName::kTouchpad]);
  return builder.GetGamepad();
}

// Note that since this is built by polling, and so eventing changes are not
// accounted for here.
std::unordered_map<ButtonName, GamepadBuilder::ButtonData> ParseButtonState(
    const WMRInputSourceState* source_state) {
  std::unordered_map<ButtonName, GamepadBuilder::ButtonData> button_map;

  // Add the select button
  GamepadBuilder::ButtonData data = button_map[ButtonName::kSelect];
  data.pressed = source_state->IsSelectPressed();
  data.touched = data.pressed;
  data.value = source_state->SelectPressedValue();
  data.has_both_axes = false;
  button_map[ButtonName::kSelect] = data;

  // Add the grip button
  data = button_map[ButtonName::kGrip];
  data.pressed = source_state->IsGrasped();
  data.touched = data.pressed;
  data.value = data.pressed ? 1.0 : 0.0;
  data.has_both_axes = false;
  button_map[ButtonName::kGrip] = data;

  // Select and grip are the only two required buttons, if we can't get the
  // others, we can safely return just them.
  if (!source_state->SupportsControllerProperties())
    return button_map;

  // Add the thumbstick
  data = button_map[ButtonName::kThumbstick];
  data.pressed = source_state->IsThumbstickPressed();
  data.touched = data.pressed;
  data.value = data.pressed ? 1.0 : 0.0;

  // TODO(https://crbug.com/966060): Determine if inverting the y value here is
  // necessary.
  data.has_both_axes = true;
  data.x_axis = source_state->ThumbstickX();
  data.y_axis = -source_state->ThumbstickY();

  button_map[ButtonName::kThumbstick] = data;

  // Add the touchpad
  data = button_map[ButtonName::kTouchpad];
  data.pressed = source_state->IsTouchpadPressed();
  data.touched = source_state->IsTouchpadTouched() || data.pressed;
  data.value = data.pressed ? 1.0 : 0.0;

  // The Touchpad does have Axes, but if it's not touched, they are 0.
  data.has_both_axes = true;
  if (data.touched) {
    // TODO(https://crbug.com/966060): Determine if inverting the y value here
    // is necessary.
    data.x_axis = source_state->TouchpadX();
    data.y_axis = -source_state->TouchpadY();
  } else {
    data.x_axis = 0;
    data.y_axis = 0;
  }

  button_map[ButtonName::kTouchpad] = data;

  return button_map;
}

gfx::Transform CreateTransform(GamepadVector position,
                               GamepadQuaternion rotation) {
  gfx::DecomposedTransform decomposed_transform;
  decomposed_transform.translate[0] = position.x;
  decomposed_transform.translate[1] = position.y;
  decomposed_transform.translate[2] = position.z;

  decomposed_transform.quaternion =
      gfx::Quaternion(rotation.x, rotation.y, rotation.z, rotation.w);
  return gfx::ComposeTransform(decomposed_transform);
}

base::Optional<gfx::Transform> TryGetGripFromPointer(
    const WMRInputSourceState* state,
    const WMRInputSource* source,
    const WMRCoordinateSystem* origin,
    gfx::Transform origin_from_grip) {
  if (!origin)
    return base::nullopt;

  // We can get the pointer position, but we'll need to transform it to an
  // offset from the grip position.  If we can't get an inverse of that,
  // then go ahead and bail early.
  gfx::Transform grip_from_origin;
  if (!origin_from_grip.GetInverse(&grip_from_origin))
    return base::nullopt;

  bool pointing_supported = source->IsPointingSupported();

  std::unique_ptr<WMRPointerPose> pointer_pose =
      state->TryGetPointerPose(origin);
  if (!pointer_pose)
    return base::nullopt;

  WFN::Vector3 pos;
  WFN::Quaternion rot;
  if (pointing_supported) {
    std::unique_ptr<WMRPointerSourcePose> pointer_source_pose =
        pointer_pose->TryGetInteractionSourcePose(source);
    if (!pointer_source_pose)
      return base::nullopt;

    pos = pointer_source_pose->Position();
    rot = pointer_source_pose->Orientation();
  } else {
    pos = pointer_pose->HeadForward();
  }

  gfx::Transform origin_from_pointer = CreateTransform(
      ConvertToGamepadVector(pos), ConvertToGamepadQuaternion(rot));
  return (grip_from_origin * origin_from_pointer);
}

device::mojom::XRHandedness WindowsToMojoHandedness(Handedness handedness) {
  switch (handedness) {
    case Handedness::SpatialInteractionSourceHandedness_Left:
      return device::mojom::XRHandedness::LEFT;
    case Handedness::SpatialInteractionSourceHandedness_Right:
      return device::mojom::XRHandedness::RIGHT;
    default:
      return device::mojom::XRHandedness::NONE;
  }
}

uint32_t GetSourceId(const WMRInputSource* source) {
  uint32_t id = source->Id();

  // Voice's ID seems to be coming through as 0, which will cause a DCHECK in
  // the hash table used on the blink side.  To ensure that we don't have any
  // collisions with other ids, increment all of the ids by one.
  id++;
  DCHECK(id != 0);

  return id;
}
}  // namespace

MixedRealityInputHelper::MixedRealityInputHelper(
    HWND hwnd,
    const base::WeakPtr<MixedRealityRenderLoop>& weak_render_loop)
    : hwnd_(hwnd),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_render_loop_(weak_render_loop),
      weak_ptr_factory_(this) {}

MixedRealityInputHelper::~MixedRealityInputHelper() {
  // Dispose must be called before destruction, which ensures that we're
  // unsubscribed from events.
  DCHECK(pressed_subscription_ == nullptr);
  DCHECK(released_subscription_ == nullptr);
}

void MixedRealityInputHelper::Dispose() {
  UnsubscribeEvents();
}

bool MixedRealityInputHelper::EnsureSpatialInteractionManager() {
  if (input_manager_)
    return true;

  if (!hwnd_)
    return false;

  input_manager_ = WMRInputManagerFactory::GetForWindow(hwnd_);

  if (!input_manager_)
    return false;

  SubscribeEvents();
  return true;
}

std::vector<mojom::XRInputSourceStatePtr>
MixedRealityInputHelper::GetInputState(const WMRCoordinateSystem* origin,
                                       const WMRTimestamp* timestamp) {
  std::vector<mojom::XRInputSourceStatePtr> input_states;

  if (!timestamp || !origin || !EnsureSpatialInteractionManager())
    return input_states;

  auto source_states =
      input_manager_->GetDetectedSourcesAtTimestamp(timestamp->GetRawPtr());

  for (const auto& state : source_states) {
    auto parsed_source_state = ParseWindowsSourceState(state.get(), origin);

    if (parsed_source_state.source_state) {
      input_states.push_back(std::move(parsed_source_state.source_state));
    }
  }

  return input_states;
}

mojom::XRGamepadDataPtr MixedRealityInputHelper::GetWebVRGamepadData(
    const WMRCoordinateSystem* origin,
    const WMRTimestamp* timestamp) {
  auto ret = mojom::XRGamepadData::New();

  if (!timestamp || !origin || !EnsureSpatialInteractionManager())
    return ret;

  auto source_states =
      input_manager_->GetDetectedSourcesAtTimestamp(timestamp->GetRawPtr());

  for (const auto& state : source_states) {
    auto parsed_source_state = ParseWindowsSourceState(state.get(), origin);

    // If we have a source_state, then we should have enough data.
    if (parsed_source_state.source_state)
      ret->gamepads.push_back(GetWebVRGamepad(std::move(parsed_source_state)));
  }

  return ret;
}

ParsedInputState MixedRealityInputHelper::ParseWindowsSourceState(
    const WMRInputSourceState* state,
    const WMRCoordinateSystem* origin) {
  ParsedInputState input_state;
  if (!origin)
    return input_state;

  std::unique_ptr<WMRInputSource> source = state->GetSource();
  SourceKind source_kind = source->Kind();

  bool is_controller =
      (source_kind == SourceKind::SpatialInteractionSourceKind_Controller);
  bool is_voice =
      (source_kind == SourceKind::SpatialInteractionSourceKind_Voice);

  if (!(is_controller || is_voice))
    return input_state;

  // Hands may not have the same id especially if they are lost but since we
  // are only tracking controllers/voice, this id should be consistent.
  uint32_t id = GetSourceId(source.get());

  // Note that if this is untracked we're not supposed to send up the "grip"
  // position, so this will be left as identity and let us still use the same
  // code paths. Any transformations will leave the original item unaffected.
  // Voice input is always untracked.
  gfx::Transform origin_from_grip;
  bool is_tracked = false;
  if (is_controller) {
    input_state.button_data = ParseButtonState(state);
    std::unique_ptr<WMRInputLocation> location_in_origin =
        state->TryGetLocation(origin);
    if (location_in_origin) {
      auto gamepad_pose = GetGamepadPose(location_in_origin.get());
      if (gamepad_pose.not_null && gamepad_pose.position.not_null &&
          gamepad_pose.orientation.not_null) {
        origin_from_grip =
            CreateTransform(gamepad_pose.position, gamepad_pose.orientation);
        is_tracked = true;
      }

      input_state.gamepad_pose = gamepad_pose;
    }

    std::unique_ptr<WMRController> controller = source->Controller();
    if (controller) {
      input_state.product_id = controller->ProductId();
      input_state.vendor_id = controller->VendorId();
    }
  }

  base::Optional<gfx::Transform> grip_from_pointer =
      TryGetGripFromPointer(state, source.get(), origin, origin_from_grip);

  // If we failed to get grip_from_pointer, see if it is cached.  If we did get
  // it, update the cache.
  if (!grip_from_pointer) {
    grip_from_pointer = controller_states_[id].grip_from_pointer;
  } else {
    controller_states_[id].grip_from_pointer = grip_from_pointer;
  }

  // Now that we have calculated information for the object, build it.
  device::mojom::XRInputSourceStatePtr source_state =
      device::mojom::XRInputSourceState::New();

  source_state->source_id = id;
  source_state->primary_input_pressed = controller_states_[id].pressed;
  source_state->primary_input_clicked = controller_states_[id].clicked;

  // Grip position should *only* be specified if the controller is tracked.
  if (is_tracked)
    source_state->grip = origin_from_grip;

  device::mojom::XRInputSourceDescriptionPtr description =
      device::mojom::XRInputSourceDescription::New();

  // If we've gotten this far we've gotten the real position.
  description->emulated_position = false;
  description->pointer_offset = grip_from_pointer;

  if (is_voice) {
    description->target_ray_mode = device::mojom::XRTargetRayMode::GAZING;
    description->handedness = device::mojom::XRHandedness::NONE;
  } else if (is_controller) {
    description->target_ray_mode = device::mojom::XRTargetRayMode::POINTING;
    description->handedness = WindowsToMojoHandedness(source->Handedness());
  } else {
    NOTREACHED();
  }

  source_state->description = std::move(description);

  input_state.source_state = std::move(source_state);

  input_state.source_state->gamepad = GetWebXRGamepad(input_state);

  return input_state;
}

void MixedRealityInputHelper::OnSourcePressed(
    const WMRInputSourceEventArgs& args) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MixedRealityInputHelper::ProcessSourceEvent,
                     weak_ptr_factory_.GetWeakPtr(), args.PressKind(),
                     args.State(), true /* is_pressed */));
}

void MixedRealityInputHelper::OnSourceReleased(
    const WMRInputSourceEventArgs& args) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MixedRealityInputHelper::ProcessSourceEvent,
                     weak_ptr_factory_.GetWeakPtr(), args.PressKind(),
                     args.State(), false /* is_pressed */));
}

void MixedRealityInputHelper::ProcessSourceEvent(
    PressKind press_kind,
    std::unique_ptr<WMRInputSourceState> state,
    bool is_pressed) {
  if (press_kind != PressKind::SpatialInteractionPressKind_Select)
    return;

  std::unique_ptr<WMRInputSource> source = state->GetSource();
  SourceKind source_kind = source->Kind();

  if (source_kind != SourceKind::SpatialInteractionSourceKind_Controller &&
      source_kind != SourceKind::SpatialInteractionSourceKind_Voice)
    return;

  uint32_t id = GetSourceId(source.get());

  bool wasPressed = controller_states_[id].pressed;
  controller_states_[id].pressed = is_pressed;
  controller_states_[id].clicked = (wasPressed && !is_pressed);

  if (!weak_render_loop_)
    return;

  auto* origin = weak_render_loop_->GetOrigin();
  if (!origin)
    return;

  auto parsed_source_state = ParseWindowsSourceState(state.get(), origin);
  if (parsed_source_state.source_state) {
    weak_render_loop_->OnInputSourceEvent(
        std::move(parsed_source_state.source_state));
  }

  // We've sent up the click, so clear it.
  controller_states_[id].clicked = false;
}

void MixedRealityInputHelper::SubscribeEvents() {
  DCHECK(input_manager_);
  DCHECK(pressed_subscription_ == nullptr);
  DCHECK(released_subscription_ == nullptr);

  // Unretained is safe since we explicitly get disposed and unsubscribe before
  // destruction
  pressed_subscription_ =
      input_manager_->AddPressedCallback(base::BindRepeating(
          &MixedRealityInputHelper::OnSourcePressed, base::Unretained(this)));
  released_subscription_ =
      input_manager_->AddReleasedCallback(base::BindRepeating(
          &MixedRealityInputHelper::OnSourceReleased, base::Unretained(this)));
}

void MixedRealityInputHelper::UnsubscribeEvents() {
  pressed_subscription_ = nullptr;
  released_subscription_ = nullptr;
}

}  // namespace device
