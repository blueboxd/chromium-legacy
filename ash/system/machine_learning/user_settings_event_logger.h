// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MACHINE_LEARNING_USER_SETTINGS_EVENT_LOGGER_H_
#define ASH_SYSTEM_MACHINE_LEARNING_USER_SETTINGS_EVENT_LOGGER_H_

#include "ash/system/machine_learning/user_settings_event.pb.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"

namespace ash {
namespace ml {

// Handler for logging user-initiated settings events to UKM.
class UserSettingsEventLogger {
 public:
  UserSettingsEventLogger(const UserSettingsEventLogger&) = delete;
  UserSettingsEventLogger& operator=(const UserSettingsEventLogger&) = delete;

  // Creates an instance of the logger. Only one instance of the logger can
  // exist in the current process.
  static void CreateInstance();
  static void DeleteInstance();
  // Gets the current instance of the logger.
  static UserSettingsEventLogger* Get();

  // Logs an event to UKM that the user has connected to the given network.
  void LogNetworkUkmEvent(
      const chromeos::network_config::mojom::NetworkStateProperties& network);

 private:
  UserSettingsEventLogger();
  ~UserSettingsEventLogger();

  // Populates contextual information shared by all settings events.
  void PopulateSharedFeatures(UserSettingsEvent* event);

  // Sends the given event to UKM.
  void SendToUkm(const UserSettingsEvent& event);

  bool used_cellular_in_session_;

  static UserSettingsEventLogger* instance_;
};

}  // namespace ml
}  // namespace ash

#endif  // ASH_SYSTEM_MACHINE_LEARNING_USER_SETTINGS_EVENT_LOGGER_H_
