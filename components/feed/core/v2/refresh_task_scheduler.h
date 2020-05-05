
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_REFRESH_TASK_SCHEDULER_H_
#define COMPONENTS_FEED_CORE_V2_REFRESH_TASK_SCHEDULER_H_

#include "base/time/time.h"

namespace feed {

// Schedules a background task for refreshing the Feed.
// When the scheduled task executes, it calls FeedStream::ExecuteRefreshTask().
class RefreshTaskScheduler {
 public:
  RefreshTaskScheduler() = default;
  virtual ~RefreshTaskScheduler() = default;

  // Schedules the task to run at |run_time|. This overrides any previous calls
  // to |EnsureScheduled|.
  virtual void EnsureScheduled(base::Time run_time) = 0;
  // Cancel the task if it was previously scheduled.
  virtual void Cancel() = 0;
  // After FeedStream::ExecuteRefreshTask is called, the callee must call this
  // function to indicate the work is complete.
  virtual void RefreshTaskComplete() = 0;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_REFRESH_TASK_SCHEDULER_H_
