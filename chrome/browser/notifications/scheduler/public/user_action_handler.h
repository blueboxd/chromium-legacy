// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_USER_ACTION_HANDLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_USER_ACTION_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"

namespace notifications {

// An interface to plumb user actions events to notification scheduling system.
// Each event needs to provide an unique id of the notification shown.
class UserActionHandler {
 public:
  // Called when the user clicks on the notification. |guid| is the internal id
  // to track the notification persist to disk.
  virtual void OnClick(SchedulerClientType type, const std::string& guid) = 0;

  // Called when the user clicks on a button on the notification.
  virtual void OnActionClick(SchedulerClientType type,
                             const std::string& guid,
                             ActionButtonType button_type) = 0;

  // Called when the user cancels or dismiss the notification.
  virtual void OnDismiss(SchedulerClientType type, const std::string& guid) = 0;

  ~UserActionHandler() = default;

 protected:
  UserActionHandler() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserActionHandler);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_USER_ACTION_HANDLER_H_
