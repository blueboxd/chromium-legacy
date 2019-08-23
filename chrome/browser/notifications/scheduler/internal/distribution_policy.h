// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_DISTRIBUTION_POLICY_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_DISTRIBUTION_POLICY_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"

namespace notifications {

// Defines how to distribute notifications to show in different background tasks
// in a day.
// TODO(xingliu): Delete this.
class DistributionPolicy {
 public:
  // Creates the default distribution policy.
  static std::unique_ptr<DistributionPolicy> Create();

  DistributionPolicy() = default;
  virtual ~DistributionPolicy() = default;

  // Returns the maximum number of notifications to show in the background task
  // starts at |task_start_time|. Suppose we at most can show |quota| number of
  // new notifications during the current background task.
  virtual int MaxToShow(SchedulerTaskTime task_start_time, int quota) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DistributionPolicy);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_DISTRIBUTION_POLICY_H_
