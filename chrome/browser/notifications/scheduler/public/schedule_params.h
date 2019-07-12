// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_SCHEDULE_PARAMS_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_SCHEDULE_PARAMS_H_

#include <map>

#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"

namespace notifications {

// Specifies when to show the scheduled notification, and throttling details.
struct ScheduleParams {
  enum class Priority {
    // Notification may be delivered if picked by display decision layer. Most
    // notification types should use this priority.
    kLow,
    // Notification may be delivered if picked by display decision layer. Has
    // higher priority to pick as the next notification to deliver. Should not
    // be used by feature frequently send notifications.
    kHigh,
    // No notification throttling logic is applied, every notification scheduled
    // will be delivered.
    kNoThrottle,
  };

  ScheduleParams();
  ScheduleParams(const ScheduleParams& other);
  bool operator==(const ScheduleParams& other) const;
  ~ScheduleParams();

  Priority priority;

  // Override the default mapping from an user action to impression result. By
  // default, click on the notification and helpful button click are positive
  // impression and may increase feature exposure. Unhelp button click is
  // negative impression and may reduce feature exposure. Dimiss/close
  // notification is neutural. Only put value when need to change the default
  // mapping.
  std::map<UserFeedback, ImpressionResult> impression_mapping;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_SCHEDULE_PARAMS_H_
