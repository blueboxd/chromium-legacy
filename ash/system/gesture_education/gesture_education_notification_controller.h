// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_GESTURE_EDUCATION_GESTURE_EDUCATION_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_GESTURE_EDUCATION_GESTURE_EDUCATION_NOTIFICATION_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/session/session_observer.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "chromeos/dbus/power/power_manager_client.h"

class PrefRegistrySimple;

namespace ash {

// Controller class to manage gesture education notification. This notification
// shows up once to provide the user with information about new gestures added
// to chrome os for easier navigation.
class ASH_EXPORT GestureEducationNotificationController
    : public SessionObserver {
 public:
  GestureEducationNotificationController();
  ~GestureEducationNotificationController() override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // See Shell::RegisterProfilePrefs().
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  void GenerateGestureEducationNotification();
  base::string16 GetNotificationTitle() const;
  base::string16 GetNotificationMessage() const;
  void HandleNotificationClick();
  void OnReceivedSwitchStates(
      base::Optional<chromeos::PowerManagerClient::SwitchStates> switch_states);

  bool tablet_mode_supported_ = false;

  base::WeakPtrFactory<GestureEducationNotificationController>
      weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_GESTURE_EDUCATION_GESTURE_EDUCATION_NOTIFICATION_CONTROLLER_H_
