// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MACHINE_LEARNING_USER_SETTINGS_EVENT_LOGGER_H_
#define ASH_SYSTEM_MACHINE_LEARNING_USER_SETTINGS_EVENT_LOGGER_H_

#include "ash/shell_observer.h"
#include "ash/system/bluetooth/tray_bluetooth_helper.h"
#include "ash/system/machine_learning/user_settings_event.pb.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"

namespace ash {
namespace ml {

// This class handles logging for settings changes that are initiated by the
// user from the quick settings tray.
class UserSettingsEventLogger
    : public ShellObserver,
      public chromeos::CrasAudioHandler::AudioObserver {
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

  // Logs an event to UKM that the user has connected to the given bluetooth
  // device.
  void LogBluetoothUkmEvent(const BluetoothAddress& device_address);

  // Logs an event to UKM that the user has toggled night light to the given
  // state.
  void LogNightLightUkmEvent(bool enabled);

  // Logs an event to UKM that the user has toggled Quiet Mode to the given
  // state.
  void LogQuietModeUkmEvent(bool enabled);

  // Logs an event to UKM that the user has changed the volume from the tray.
  void LogVolumeUkmEvent(int previous_level, int current_level);

  // Logs an event to UKM that the user has changed the brightness from the
  // tray.
  void LogBrightnessUkmEvent(int previous_level, int current_level);

  // ShellObserver overrides:
  void OnCastingSessionStartedOrStopped(bool started) override;
  void OnFullscreenStateChanged(bool is_fullscreen,
                                aura::Window* container) override;

  // chromeos::CrasAudioHandler::AudioObserver overrides:
  void OnOutputStarted() override;
  void OnOutputStopped() override;

 private:
  UserSettingsEventLogger();
  ~UserSettingsEventLogger() override;

  // Populates contextual information shared by all settings events.
  void PopulateSharedFeatures(UserSettingsEvent* event);

  // Sends the given event to UKM.
  void SendToUkm(const UserSettingsEvent& event);

  void OnPresentingTimerEnded();
  void OnFullscreenTimerEnded();

  base::OneShotTimer presenting_timer_;
  base::OneShotTimer fullscreen_timer_;
  int presenting_session_count_;
  // Whether the device has been presenting in the last 5 minutes.
  bool is_recently_presenting_;
  // Whether the device has been in fullscreen mode in the last 5 minutes.
  bool is_recently_fullscreen_;

  bool used_cellular_in_session_;
  bool is_playing_audio_;

  SEQUENCE_CHECKER(sequence_checker_);

  static UserSettingsEventLogger* instance_;
};

}  // namespace ml
}  // namespace ash

#endif  // ASH_SYSTEM_MACHINE_LEARNING_USER_SETTINGS_EVENT_LOGGER_H_
