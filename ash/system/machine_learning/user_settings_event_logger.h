// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MACHINE_LEARNING_USER_SETTINGS_EVENT_LOGGER_H_
#define ASH_SYSTEM_MACHINE_LEARNING_USER_SETTINGS_EVENT_LOGGER_H_

#include "ash/system/machine_learning/user_settings_event.pb.h"

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
  static UserSettingsEventLogger* Get();

  // Logs a settings change event to UKM.
  void LogUkmEvent();

 private:
  UserSettingsEventLogger();
  ~UserSettingsEventLogger();

  static UserSettingsEventLogger* instance_;
};

}  // namespace ml
}  // namespace ash

#endif  // ASH_SYSTEM_MACHINE_LEARNING_USER_SETTINGS_EVENT_LOGGER_H_
