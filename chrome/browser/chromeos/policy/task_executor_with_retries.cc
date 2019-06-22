// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/task_executor_with_retries.h"

#include <utility>

namespace policy {

TaskExecutorWithRetries::TaskExecutorWithRetries(
    const std::string& description,
    GetTicksSinceBootFn get_ticks_since_boot_fn,
    int max_retries,
    base::TimeDelta retry_time)
    : description_(description),
      get_ticks_since_boot_fn_(std::move(get_ticks_since_boot_fn)),
      max_retries_(max_retries),
      retry_time_(retry_time) {}

TaskExecutorWithRetries::~TaskExecutorWithRetries() = default;

void TaskExecutorWithRetries::Start(AsyncTask task,
                                    RetryFailureCb retry_failure_cb) {
  ResetState();
  task_ = std::move(task);
  retry_failure_cb_ = std::move(retry_failure_cb);
  task_.Run();
}

void TaskExecutorWithRetries::Stop() {
  ResetState();
}

void TaskExecutorWithRetries::ScheduleRetry() {
  // This method may be called from |retry_timer_|'s |OnExpiration| method. It's
  // best to schedule |RetryTask| as a separate task as it destroys the same
  // |NativeTimer| object inside it. For easier state maintenance there can only
  // be one retry task pending at a time.
  weak_factory_.InvalidateWeakPtrs();
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&TaskExecutorWithRetries::RetryTask,
                                weak_factory_.GetWeakPtr()));
}

void TaskExecutorWithRetries::OnRetryTimerStartResult(bool result) {
  if (!result) {
    LOG(ERROR) << "Failed to start retry timer";
    ScheduleRetry();
    return;
  }
}

void TaskExecutorWithRetries::ResetState() {
  num_retries_ = 0;
  task_.Reset();
  retry_failure_cb_.Reset();
  retry_timer_.reset();
  weak_factory_.InvalidateWeakPtrs();
}

void TaskExecutorWithRetries::RetryTask() {
  // Retrying has a limit. In the unlikely scenario this is met, reset all
  // state. Now an update check can only happen when a new policy comes in or
  // Chrome is restarted.
  if (num_retries_ >= max_retries_) {
    LOG(ERROR) << "Task runner " << description_
               << " aborting task after max retries";
    std::move(retry_failure_cb_).Run();
    ResetState();
    return;
  }

  // There can only be one pending call to |task_| at any given time, delete any
  // pending calls by resetting the timer. The old timer must be destroyed
  // before creating a new timer with the same tag as per the semantics of
  // |NativeTimer|. That's why using std::make_unique with the assignment
  // operator would not have worked here. Safe to use "this" for the result
  // callback as |retry_timer_| can't outlive its parent.
  ++num_retries_;
  retry_timer_.reset();
  retry_timer_ = std::make_unique<chromeos::NativeTimer>(description_);
  retry_timer_->Start(
      get_ticks_since_boot_fn_.Run() + retry_time_, task_,
      base::BindOnce(&TaskExecutorWithRetries::OnRetryTimerStartResult,
                     base::Unretained(this)));
}

}  // namespace policy
