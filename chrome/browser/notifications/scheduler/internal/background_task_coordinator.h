// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_BACKGROUND_TASK_COORDINATOR_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_BACKGROUND_TASK_COORDINATOR_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"

namespace base {
class Clock;
}  // namespace base

namespace notifications {

class NotificationBackgroundTaskScheduler;
struct ClientState;
struct NotificationEntry;
struct SchedulerConfig;

// Schedules background task at the right time based on scheduled notification
// data and impression data.
class BackgroundTaskCoordinator {
 public:
  using Notifications =
      std::map<SchedulerClientType, std::vector<const NotificationEntry*>>;
  using ClientStates = std::map<SchedulerClientType, const ClientState*>;
  using TimeRandomizer = base::RepeatingCallback<base::TimeDelta()>;
  static base::TimeDelta DefaultTimeRandomizer(
      const base::TimeDelta& time_window);
  BackgroundTaskCoordinator(
      std::unique_ptr<NotificationBackgroundTaskScheduler> background_task,
      const SchedulerConfig* config,
      TimeRandomizer time_randomizer,
      base::Clock* clock);
  virtual ~BackgroundTaskCoordinator();

  // Schedule background task based on current notification in the storage.
  virtual void ScheduleBackgroundTask(Notifications notifications,
                                      ClientStates client_states,
                                      SchedulerTaskTime task_start_time);

 private:
  // The class that actually schedules platform background task.
  std::unique_ptr<NotificationBackgroundTaskScheduler> background_task_;

  // System configuration.
  const SchedulerConfig* config_;

  // Randomize the time to show the notification, to avoid large number of users
  // to perform actions at the same time.
  TimeRandomizer time_randomizer_;

  // Clock to query the current timestamp.
  base::Clock* clock_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundTaskCoordinator);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_BACKGROUND_TASK_COORDINATOR_H_
