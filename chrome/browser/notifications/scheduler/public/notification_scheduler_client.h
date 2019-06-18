// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_NOTIFICATION_SCHEDULER_CLIENT_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_NOTIFICATION_SCHEDULER_CLIENT_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/notifications/scheduler/public/notification_data.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace notifications {

// The client interface to receive events from notification scheduler.
class NotificationSchedulerClient {
 public:
  struct DisplayData {
    NotificationData notification_data;
    SkBitmap icon;
  };

  // Defines user actions type.
  enum class UserActionType {
    // The user clicks on the notification body.
    kClick = 0,
    // The user clicks on the notification button.
    kButtonClick = 1,
    // The user dismisses the notification.
    kDismiss = 2,
  };

  // Information about button clicks.
  struct ButtonClickInfo {
    // Unique id of the button.
    std::string button_id;

    // Associate impression type for the button.
    ActionButtonType type = ActionButtonType::kUnknownAction;
  };

  using DisplayCallback =
      base::OnceCallback<void(std::unique_ptr<DisplayData>)>;

  NotificationSchedulerClient() = default;
  virtual ~NotificationSchedulerClient() = default;

  // Called when the notification should be displayed to the user. The clients
  // can overwrite data in |display_data| and return the updated data in
  // |callback|.
  virtual void ShowNotification(std::unique_ptr<DisplayData> display_data,
                                DisplayCallback callback) = 0;

  // Called when scheduler is initialized, number of notification scheduled for
  // this type is reported if initialization succeeded.
  virtual void OnSchedulerInitialized(bool success,
                                      std::set<std::string> guids) = 0;

  // Called when the user interacts with the notification.
  virtual void OnUserAction(UserActionType action_type,
                            const std::string& notification_id,
                            base::Optional<ButtonClickInfo> button_info) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationSchedulerClient);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_NOTIFICATION_SCHEDULER_CLIENT_H_
