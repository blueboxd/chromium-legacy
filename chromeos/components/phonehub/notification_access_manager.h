// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_ACCESS_MANAGER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_ACCESS_MANAGER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace chromeos {
namespace phonehub {

// Tracks the status of whether the user has enabled notification access on
// their phone. While Phone Hub can be enabled via Chrome OS, access to
// notifications requires that the user grant access via Android settings. If a
// Phone Hub connection to the phone has never succeeded, we assume that access
// has not yet been granted. If there is no active Phone Hub connection, we
// assume that the last access value seen is the current value.
class NotificationAccessManager {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when notification access has changed; use HasAccessBeenGranted()
    // for the new status.
    virtual void OnNotificationAccessChanged() = 0;
  };

  NotificationAccessManager(const NotificationAccessManager&) = delete;
  NotificationAccessManager& operator=(const NotificationAccessManager&) =
      delete;
  virtual ~NotificationAccessManager();

  virtual bool HasAccessBeenGranted() const = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  NotificationAccessManager();

  void NotifyNotificationAccessChanged();

 private:
  base::ObserverList<Observer> observer_list_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_ACCESS_MANAGER_H_
