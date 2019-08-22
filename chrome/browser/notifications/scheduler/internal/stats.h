// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_STATS_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_STATS_H_

#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"

namespace notifications {
namespace stats {

// Logs the user action when the user interacts with notification sent from the
// scheduling system.
void LogUserAction(const UserActionData& user_action_data);

// Logs the initialization result for impression database.
void LogImpressionDbInit(bool success, int entry_count);

// Logs impression db operations result except the initialization.
void LogImpressionDbOperation(bool success);

// Logs the initialization result for notification database.
void LogNotificationDbInit(bool success, int entry_count);

// Logs notification db operations result except the initialization.
void LogNotificationDbOperation(bool success);

}  // namespace stats
}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_STATS_H_
