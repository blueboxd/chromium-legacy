// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EOL_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_EOL_NOTIFICATION_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "chrome/browser/profiles/profile.h"
#include "third_party/cros_system_api/dbus/update_engine/dbus-constants.h"

namespace base {
class Clock;
}  // namespace base

namespace chromeos {

// EolNotification is created when user logs in. It is
// used to check current EndOfLife Status of the device,
// and show notification accordingly.
class EolNotification final {
 public:
  // Returns true if the eol notification needs to be displayed.
  static bool ShouldShowEolNotification();

  explicit EolNotification(Profile* profile);
  ~EolNotification();

  // Check Eol status from update engine.
  void CheckEolStatus();

 private:
  friend class EolNotificationTest;

  // Overridden for testing Milestones until EOL notifications.
  base::Clock* clock_;

  // Callback invoked when |GetEolStatus()| has finished.
  // - EndOfLife status: the end of life status of the device.
  // - Optional<int32_t> number_of_milestones: the number of milestones before
  // end of life, if known by update engine.
  void OnEolStatus(update_engine::EndOfLifeStatus status,
                   base::Optional<int32_t> number_of_milestones);

  // Create or updates the notfication.
  void Update();

  // Profile which is associated with the EndOfLife notification.
  Profile* const profile_;

  // Device EndOfLife status.
  update_engine::EndOfLifeStatus status_;

  // Number of milestones remaining until end of life.
  base::Optional<int32_t> number_of_milestones_;

  // Factory of callbacks.
  base::WeakPtrFactory<EolNotification> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(EolNotification);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EOL_NOTIFICATION_H_
