// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/isolated_gamepad_data_fetcher.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/buildflag.h"
#include "device/vr/buildflags/buildflags.h"
#include "device/vr/vr_device.h"

namespace device {

namespace {

bool IsValidDeviceId(device::mojom::XRDeviceId id) {
#if BUILDFLAG(ENABLE_OPENVR)
  if (id == device::mojom::XRDeviceId::OPENVR_DEVICE_ID)
    return true;
#endif

#if BUILDFLAG(ENABLE_OCULUS_VR)
  if (id == device::mojom::XRDeviceId::OCULUS_DEVICE_ID)
    return true;
#endif

#if BUILDFLAG(ENABLE_WINDOWS_MR)
  if (id == device::mojom::XRDeviceId::WINDOWS_MIXED_REALITY_ID)
    return true;
#endif

  return false;
}

GamepadSource GamepadSourceFromDeviceId(device::mojom::XRDeviceId id) {
  DCHECK(IsValidDeviceId(id));

#if BUILDFLAG(ENABLE_OPENVR)
  if (id == device::mojom::XRDeviceId::OPENVR_DEVICE_ID)
    return GAMEPAD_SOURCE_OPENVR;
#endif

#if BUILDFLAG(ENABLE_OCULUS_VR)
  if (id == device::mojom::XRDeviceId::OCULUS_DEVICE_ID)
    return GAMEPAD_SOURCE_OCULUS;
#endif

#if BUILDFLAG(ENABLE_WINDOWS_MR)
  if (id == device::mojom::XRDeviceId::WINDOWS_MIXED_REALITY_ID)
    return GAMEPAD_SOURCE_WIN_MR;
#endif

  NOTREACHED();
  return GAMEPAD_SOURCE_NONE;
}

}  // namespace

IsolatedGamepadDataFetcher::Factory::Factory(
    device::mojom::XRDeviceId display_id,
    device::mojom::IsolatedXRGamepadProviderFactoryPtr factory)
    : display_id_(display_id), factory_(std::move(factory)) {}

IsolatedGamepadDataFetcher::Factory::~Factory() {}

std::unique_ptr<GamepadDataFetcher>
IsolatedGamepadDataFetcher::Factory::CreateDataFetcher() {
  device::mojom::IsolatedXRGamepadProviderPtr provider;
  factory_->GetIsolatedXRGamepadProvider(mojo::MakeRequest(&provider));
  return std::make_unique<IsolatedGamepadDataFetcher>(display_id_,
                                                      std::move(provider));
}

GamepadSource IsolatedGamepadDataFetcher::Factory::source() {
  return GamepadSourceFromDeviceId(display_id_);
}

IsolatedGamepadDataFetcher::IsolatedGamepadDataFetcher(
    device::mojom::XRDeviceId display_id,
    device::mojom::IsolatedXRGamepadProviderPtr provider)
    : display_id_(display_id) {
  // We bind provider_ on the poling thread, but we're created on the main UI
  // thread.
  provider_info_ = provider.PassInterface();
}

IsolatedGamepadDataFetcher::~IsolatedGamepadDataFetcher() = default;

GamepadSource IsolatedGamepadDataFetcher::source() {
  return GamepadSourceFromDeviceId(display_id_);
}

void IsolatedGamepadDataFetcher::OnDataUpdated(
    device::mojom::XRGamepadDataPtr data) {
  data_ = std::move(data);
  have_outstanding_request_ = false;
}

GamepadPose GamepadPoseFromXRPose(device::mojom::VRPose* pose) {
  GamepadPose ret = {};
  ret.not_null = pose;
  if (!pose) {
    return ret;
  }

  if (pose->position) {
    ret.position.not_null = true;
    ret.position.x = pose->position->x();
    ret.position.y = pose->position->y();
    ret.position.z = pose->position->z();
  }

  if (pose->orientation) {
    ret.orientation.not_null = true;
    ret.orientation.x = pose->orientation->x();
    ret.orientation.y = pose->orientation->y();
    ret.orientation.z = pose->orientation->z();
    ret.orientation.w = pose->orientation->w();
  }

  if (pose->angular_velocity) {
    ret.angular_velocity.not_null = true;
    ret.angular_velocity.x = pose->angular_velocity->x();
    ret.angular_velocity.y = pose->angular_velocity->y();
    ret.angular_velocity.z = pose->angular_velocity->z();
  }

  if (pose->linear_velocity) {
    ret.linear_velocity.not_null = true;
    ret.linear_velocity.x = pose->linear_velocity->x();
    ret.linear_velocity.y = pose->linear_velocity->y();
    ret.linear_velocity.z = pose->linear_velocity->z();
  }

  if (pose->angular_acceleration) {
    ret.angular_acceleration.not_null = true;
    ret.angular_acceleration.x = pose->angular_acceleration->x();
    ret.angular_acceleration.y = pose->angular_acceleration->y();
    ret.angular_acceleration.z = pose->angular_acceleration->z();
  }

  if (pose->linear_acceleration) {
    ret.linear_acceleration.not_null = true;
    ret.linear_acceleration.x = pose->linear_acceleration->x();
    ret.linear_acceleration.y = pose->linear_acceleration->y();
    ret.linear_acceleration.z = pose->linear_acceleration->z();
  }

  return ret;
}

void IsolatedGamepadDataFetcher::GetGamepadData(bool devices_changed_hint) {
  if (!provider_ && provider_info_) {
    provider_.Bind(std::move(provider_info_));
  }

  // If we don't have a provider, we can't give out data.
  if (!provider_) {
    return;
  }

  // Request new data.
  if (!have_outstanding_request_) {
    have_outstanding_request_ = true;
    provider_->RequestUpdate(base::BindOnce(
        &IsolatedGamepadDataFetcher::OnDataUpdated, base::Unretained(this)));
  }

  // If we have no data to give out, nothing to do.
  if (!data_) {
    return;
  }

  // Keep track of gamepads that go missing.
  std::set<unsigned int> seen_gamepads;

  // Give out data_, but we need to translate it.
  for (size_t i = 0; i < data_->gamepads.size(); ++i) {
    auto& source = data_->gamepads[i];
    PadState* state = GetPadState(source->controller_id);
    if (!state)
      continue;
    Gamepad& dest = state->data;
    dest.connected = true;

    seen_gamepads.insert(source->controller_id);
    dest.timestamp = TimeInMicroseconds(source->timestamp);
    // WebVR did not support mapping.
    dest.mapping = GamepadMapping::kNone;
    dest.pose = GamepadPoseFromXRPose(source->pose.get());
    dest.pose.has_position = source->can_provide_position;
    dest.pose.has_orientation = source->can_provide_orientation;
    dest.hand = source->hand == device::mojom::XRHandedness::LEFT
                    ? GamepadHand::kLeft
                    : source->hand == device::mojom::XRHandedness::RIGHT
                          ? GamepadHand::kRight
                          : GamepadHand::kNone;
    dest.display_id = static_cast<unsigned int>(display_id_);
    dest.is_xr = true;

    dest.axes_length = source->axes.size();
    for (size_t j = 0; j < source->axes.size(); ++j) {
      dest.axes[j] = source->axes[j];
    }

    dest.buttons_length = source->buttons.size();
    for (size_t j = 0; j < source->buttons.size(); ++j) {
      dest.buttons[j] =
          GamepadButton(source->buttons[j]->pressed,
                        source->buttons[j]->touched, source->buttons[j]->value);
    }

    // Gamepad extensions refer to display_id, corresponding to the WebVR
    // VRDisplay's dipslayId property.
    // As WebVR is deprecated, and we only hand out a maximum of one "display"
    // per runtime, we use the XRDeviceId now to associate the controller with
    // a headset.  This doesn't change behavior, but the device/display naming
    // could be confusing here.
    if (this->source() == GAMEPAD_SOURCE_OPENVR) {
      dest.SetID(base::UTF8ToUTF16("OpenVR Gamepad"));
    } else if (this->source() == GAMEPAD_SOURCE_OCULUS) {
      if (dest.hand == GamepadHand::kLeft) {
        dest.SetID(base::UTF8ToUTF16("Oculus Touch (Left)"));
      } else if (dest.hand == GamepadHand::kRight) {
        dest.SetID(base::UTF8ToUTF16("Oculus Touch (Right)"));
      } else {
        dest.SetID(base::UTF8ToUTF16("Oculus Remote"));
      }
    } else if (this->source() == GAMEPAD_SOURCE_WIN_MR) {
      // For compatibility with Edge and existing libraries, Win MR may plumb
      // an input_state and corresponding gamepad up via the Pose for the
      // purposes of exposing the Gamepad ID that should be used.
      // If it is present, use that, otherwise, just use the same prefix as
      // Edge uses.
      if (source->pose && source->pose->input_state &&
          source->pose->input_state.value().size() > 0 &&
          source->pose->input_state.value()[0]->gamepad) {
        Gamepad id_gamepad =
            source->pose->input_state.value()[0]->gamepad.value();
        dest.SetID(id_gamepad.id);
      } else {
        dest.SetID(base::UTF8ToUTF16(
            "Spatial Controller (Spatial Interaction Source) 0000-0000"));
      }
    }

    TRACE_COUNTER1(
        "input", "XR gamepad sample age (ms)",
        (base::TimeTicks::Now() - source->timestamp).InMilliseconds());
  }

  // Remove any gamepads that aren't connected.
  for (auto id : active_gamepads_) {
    if (seen_gamepads.find(id) == seen_gamepads.end()) {
      PadState* state = GetPadState(id);
      if (state) {
        state->data.connected = false;
      }
    }
  }
  active_gamepads_ = std::move(seen_gamepads);
}

void IsolatedGamepadDataFetcher::PauseHint(bool paused) {}

void IsolatedGamepadDataFetcher::OnAddedToProvider() {}

void IsolatedGamepadDataFetcher::Factory::RemoveGamepad(
    device::mojom::XRDeviceId id) {
  if (!IsValidDeviceId(id))
    return;

  GamepadDataFetcherManager::GetInstance()->RemoveSourceFactory(
      GamepadSourceFromDeviceId(id));
}

void IsolatedGamepadDataFetcher::Factory::AddGamepad(
    device::mojom::XRDeviceId device_id,
    device::mojom::IsolatedXRGamepadProviderFactoryPtr gamepad_factory) {
  if (!IsValidDeviceId(device_id))
    return;

  device::GamepadDataFetcherManager::GetInstance()->AddFactory(
      new device::IsolatedGamepadDataFetcher::Factory(
          device_id, std::move(gamepad_factory)));
}

}  // namespace device
