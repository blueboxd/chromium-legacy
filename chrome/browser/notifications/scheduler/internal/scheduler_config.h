// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_SCHEDULER_CONFIG_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_SCHEDULER_CONFIG_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"

namespace notifications {

// Configuration of notification scheduler system.
struct SchedulerConfig {
  // Creates a default scheduler config.
  static std::unique_ptr<SchedulerConfig> Create();

  SchedulerConfig();
  ~SchedulerConfig();

  // Maximum number of all types of notifications shown to the user per day.
  int max_daily_shown_all_type;

  // Maximum number of notifications shown to the user per day for each type.
  int max_daily_shown_per_type;

  // The initial number of notifications shown to the user per day for each
  // type.
  int initial_daily_shown_per_type;

  // The time for a notification entry to expire. The
  // notification entry will be deleted then.
  base::TimeDelta notification_expiration;

  // The time for a notification impression history data to expire. The
  // impression history will be deleted then.
  base::TimeDelta impression_expiration;

  // Duration of suppression when negative impression is applied.
  base::TimeDelta suppression_duration;

  // The number of consecutive notification dismisses to generate a dismiss
  // event.
  int dismiss_count;

  // Used to check whether |dismiss_count| consecutive notification dimisses are
  // in this duration, to generate a dismiss event.
  base::TimeDelta dismiss_duration;

  // The hour (from 0 to 23) to run the morning background task for notification
  // scheduler.
  int morning_task_hour;

  // The hour (from 0 to 23) to run the evening background task for notification
  // scheduler.
  int evening_task_hour;

  // The time window to launch the background task.
  base::TimeDelta background_task_window_duration;

  // A random time delta to distribute the user clicks to a time window.
  base::TimeDelta background_task_random_time_window;

 private:
  DISALLOW_COPY_AND_ASSIGN(SchedulerConfig);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_SCHEDULER_CONFIG_H_
