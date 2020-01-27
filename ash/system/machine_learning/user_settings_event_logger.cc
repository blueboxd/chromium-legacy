// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/machine_learning/user_settings_event_logger.h"

#include "ash/shell.h"
#include "ash/system/night_light/night_light_controller_impl.h"
#include "base/logging.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"

namespace ash {
namespace ml {

using chromeos::network_config::mojom::NetworkStateProperties;
using chromeos::network_config::mojom::NetworkType;
using chromeos::network_config::mojom::SecurityType;

// static
UserSettingsEventLogger* UserSettingsEventLogger::instance_ = nullptr;

// static
void UserSettingsEventLogger::CreateInstance() {
  DCHECK(!instance_);
  instance_ = new UserSettingsEventLogger();
}

// static
void UserSettingsEventLogger::DeleteInstance() {
  delete instance_;
  instance_ = nullptr;
}

// static
UserSettingsEventLogger* UserSettingsEventLogger::Get() {
  return instance_;
}

UserSettingsEventLogger::UserSettingsEventLogger()
    : presenting_session_count_(0),
      is_recently_presenting_(false),
      is_recently_fullscreen_(false),
      used_cellular_in_session_(false),
      is_playing_audio_(false) {
  Shell::Get()->AddShellObserver(this);
  chromeos::CrasAudioHandler::Get()->AddAudioObserver(this);
}

UserSettingsEventLogger::~UserSettingsEventLogger() {
  Shell::Get()->RemoveShellObserver(this);
  chromeos::CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

void UserSettingsEventLogger::LogNetworkUkmEvent(
    const NetworkStateProperties& network) {
  UserSettingsEvent settings_event;
  auto* const event = settings_event.mutable_event();
  auto* const features = settings_event.mutable_features();

  if (network.type == NetworkType::kWiFi) {
    event->set_setting_id(UserSettingsEvent::Event::WIFI);
    const auto& wifi_state = network.type_state->get_wifi();
    features->set_signal_strength(wifi_state->signal_strength);
    features->set_has_wifi_security(wifi_state->security !=
                                    SecurityType::kNone);
  } else if (network.type == NetworkType::kCellular) {
    event->set_setting_id(UserSettingsEvent::Event::CELLULAR);
    features->set_signal_strength(
        network.type_state->get_cellular()->signal_strength);
    features->set_used_cellular_in_session(used_cellular_in_session_);
    used_cellular_in_session_ = true;
  } else {
    // We are not interested in other types of networks.
    return;
  }
  event->set_setting_type(UserSettingsEvent::Event::QUICK_SETTINGS);

  PopulateSharedFeatures(&settings_event);
  SendToUkm(settings_event);
}

void UserSettingsEventLogger::LogBluetoothUkmEvent(
    const BluetoothAddress& device_address) {
  UserSettingsEvent settings_event;
  auto* const event = settings_event.mutable_event();

  event->set_setting_id(UserSettingsEvent::Event::BLUETOOTH);
  event->set_setting_type(UserSettingsEvent::Event::QUICK_SETTINGS);

  const auto& devices =
      Shell::Get()->tray_bluetooth_helper()->GetAvailableBluetoothDevices();
  for (const auto& device : devices) {
    if (device->address == device_address) {
      settings_event.mutable_features()->set_is_paired_bluetooth_device(
          device->is_paired);
      break;
    }
  }

  PopulateSharedFeatures(&settings_event);
  SendToUkm(settings_event);
}

void UserSettingsEventLogger::LogNightLightUkmEvent(const bool enabled) {
  UserSettingsEvent settings_event;
  auto* const event = settings_event.mutable_event();
  auto* const features = settings_event.mutable_features();

  event->set_setting_id(UserSettingsEvent::Event::NIGHT_LIGHT);
  event->set_setting_type(UserSettingsEvent::Event::QUICK_SETTINGS);
  // Convert the setting state to an int. Some settings have multiple states, so
  // all setting states are stored as ints.
  event->set_previous_value(!enabled ? 1 : 0);
  event->set_current_value(enabled ? 1 : 0);

  const auto& schedule_type =
      Shell::Get()->night_light_controller()->GetScheduleType();
  features->set_has_night_light_schedule(
      schedule_type != NightLightController::ScheduleType::kNone);
  // TODO(crbug/1014839): Set the |is_after_sunset| feature field.

  PopulateSharedFeatures(&settings_event);
  SendToUkm(settings_event);
}

void UserSettingsEventLogger::LogQuietModeUkmEvent(const bool enabled) {
  UserSettingsEvent settings_event;
  auto* const event = settings_event.mutable_event();

  event->set_setting_id(UserSettingsEvent::Event::DO_NOT_DISTURB);
  event->set_setting_type(UserSettingsEvent::Event::QUICK_SETTINGS);
  // Convert the setting state to an int. Some settings have multiple states, so
  // all setting states are stored as ints.
  event->set_previous_value(!enabled ? 1 : 0);
  event->set_current_value(enabled ? 1 : 0);

  settings_event.mutable_features()->set_is_recently_presenting(
      is_recently_presenting_);

  PopulateSharedFeatures(&settings_event);
  SendToUkm(settings_event);
}

void UserSettingsEventLogger::LogVolumeUkmEvent(const int previous_level,
                                                const int current_level) {
  UserSettingsEvent settings_event;
  auto* const event = settings_event.mutable_event();

  event->set_setting_id(UserSettingsEvent::Event::VOLUME);
  event->set_setting_type(UserSettingsEvent::Event::QUICK_SETTINGS);
  event->set_previous_value(previous_level);
  event->set_current_value(current_level);

  settings_event.mutable_features()->set_is_playing_audio(is_playing_audio_);

  PopulateSharedFeatures(&settings_event);
  SendToUkm(settings_event);
}

void UserSettingsEventLogger::LogBrightnessUkmEvent(const int previous_level,
                                                    const int current_level) {
  UserSettingsEvent settings_event;
  auto* const event = settings_event.mutable_event();

  event->set_setting_id(UserSettingsEvent::Event::BRIGHTNESS);
  event->set_setting_type(UserSettingsEvent::Event::QUICK_SETTINGS);
  event->set_previous_value(previous_level);
  event->set_current_value(current_level);

  settings_event.mutable_features()->set_is_recently_fullscreen(
      is_recently_fullscreen_);

  PopulateSharedFeatures(&settings_event);
  SendToUkm(settings_event);
}

void UserSettingsEventLogger::OnCastingSessionStartedOrStopped(
    const bool started) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (started) {
    ++presenting_session_count_;
    is_recently_presenting_ = true;
    presenting_timer_.Stop();
  } else {
    --presenting_session_count_;
    DCHECK_GE(presenting_session_count_, 0);
    if (presenting_session_count_ == 0) {
      presenting_timer_.Start(FROM_HERE, base::TimeDelta::FromMinutes(5), this,
                              &UserSettingsEventLogger::OnPresentingTimerEnded);
    }
  }
}

void UserSettingsEventLogger::OnPresentingTimerEnded() {
  is_recently_presenting_ = false;
}

void UserSettingsEventLogger::OnFullscreenStateChanged(
    const bool is_fullscreen,
    aura::Window* /*container*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_fullscreen) {
    is_recently_fullscreen_ = true;
    fullscreen_timer_.Stop();
  } else {
    fullscreen_timer_.Start(FROM_HERE, base::TimeDelta::FromMinutes(5), this,
                            &UserSettingsEventLogger::OnFullscreenTimerEnded);
  }
}

void UserSettingsEventLogger::OnFullscreenTimerEnded() {
  is_recently_fullscreen_ = false;
}

void UserSettingsEventLogger::OnOutputStarted() {
  is_playing_audio_ = true;
}

void UserSettingsEventLogger::OnOutputStopped() {
  is_playing_audio_ = false;
}

void UserSettingsEventLogger::PopulateSharedFeatures(UserSettingsEvent* event) {
  // TODO(crbug/1014839): Populate the shared contextual features.
}

void UserSettingsEventLogger::SendToUkm(const UserSettingsEvent& event) {
  // TODO(crbug/1014839): Implement UKM logging.
}

}  // namespace ml
}  // namespace ash
