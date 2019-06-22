// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_TASK_EXECUTOR_WITH_RETRIES_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_TASK_EXECUTOR_WITH_RETRIES_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/native_timer.h"

namespace policy {

// This class runs a task that can fail. In case of failure it retries the task
// using NativeTimer i.e. the retry can wake the device up from suspend and is
// suspend aware. Any callbacks passed to its API will not be invoked if an
// object of this class is destroyed.
class TaskExecutorWithRetries {
 public:
  using AsyncTask = base::RepeatingClosure;
  using RetryFailureCb = base::OnceClosure;
  using GetTicksSinceBootFn = base::RepeatingCallback<base::TimeTicks(void)>;

  // |description| - String identifying this object.
  // |get_ticks_since_boot_fn| - Callback that returns current ticks from boot.
  // Used for scheduling retry timer.
  // |max_retries| - Maximum number of retries after which trying the task is
  // given up.
  // |retry_time| - Time between each retry.
  TaskExecutorWithRetries(const std::string& description,
                          GetTicksSinceBootFn get_ticks_since_boot_fn,
                          int max_retries,
                          base::TimeDelta retry_time);
  ~TaskExecutorWithRetries();

  // Runs |task| and caches |retry_failure_cb| which will be called when
  // |max_retries_| is reached and |task_| couldn't be run successfully.
  // Consecutive calls override any state and pending callbacks associated with
  // the previous call.
  void Start(AsyncTask task, RetryFailureCb retry_failure_cb);

  // Resets state and stops all pending callbacks.
  void Stop();

  // Cancels all outstanding |RetryTask| calls and schedules a new |RetryTask|
  // call on the calling sequence.
  void ScheduleRetry();

 private:
  // Called upon starting |retry_timer_|. Indicates whether or not the timer was
  // started successfully.
  void OnRetryTimerStartResult(bool result);

  // Resets state including stopping all pending callbacks.
  void ResetState();

  // Starts |retry_timer_| to schedule |task_| at |retry_time_| interval.
  void RetryTask();

  // String identifying this object. Used with the |retry_timer_| API.
  const std::string description_;

  // Used to get current time ticks from boot.
  const GetTicksSinceBootFn get_ticks_since_boot_fn_;

  // Maximum number of retries after which trying the task is given up.
  const int max_retries_;

  // Time between each retry.
  const base::TimeDelta retry_time_;

  // Current retry iteration. Capped at |max_retries_|.
  int num_retries_ = 0;

  // The task to run.
  AsyncTask task_;

  // Callback to call after |max_retries_| have been reached and |task_| wasn't
  // successfully scheduled.
  RetryFailureCb retry_failure_cb_;

  // Timer used to retry |task_|.
  std::unique_ptr<chromeos::NativeTimer> retry_timer_;

  base::WeakPtrFactory<TaskExecutorWithRetries> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TaskExecutorWithRetries);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_TASK_EXECUTOR_WITH_RETRIES_H_
