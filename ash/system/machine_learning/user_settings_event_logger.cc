// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/machine_learning/user_settings_event_logger.h"

#include "ash/shell.h"
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
    : used_cellular_in_session_(false) {}

UserSettingsEventLogger::~UserSettingsEventLogger() = default;

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
  // Convert the setting state to an int. Some settings have multiple states, so
  // all setting states are stored as ints.
  event->set_current_value(1);

  PopulateSharedFeatures(&settings_event);
  SendToUkm(settings_event);
}

void UserSettingsEventLogger::PopulateSharedFeatures(UserSettingsEvent* event) {
  // TODO(crbug/1014839): Populate the shared contextual features.
}

void UserSettingsEventLogger::SendToUkm(const UserSettingsEvent& event) {
  // TODO(crbug/1014839): Implement UKM logging.
}

}  // namespace ml
}  // namespace ash
