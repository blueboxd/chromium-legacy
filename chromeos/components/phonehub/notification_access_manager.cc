// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/notification_access_manager.h"

namespace chromeos {
namespace phonehub {

NotificationAccessManager::NotificationAccessManager() = default;

NotificationAccessManager::~NotificationAccessManager() = default;

void NotificationAccessManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void NotificationAccessManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void NotificationAccessManager::NotifyNotificationAccessChanged() {
  for (auto& observer : observer_list_)
    observer.OnNotificationAccessChanged();
}

}  // namespace phonehub
}  // namespace chromeos
