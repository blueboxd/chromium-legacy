// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_POST_JOB_H_
#define BASE_TASK_POST_JOB_H_

#include "base/base_export.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"

namespace base {
namespace internal {
class JobTaskSource;
class PooledTaskRunnerDelegate;
}
namespace experimental {

// Delegate that's passed to Job's worker task, providing an entry point to
// communicate with the scheduler.
class BASE_EXPORT JobDelegate {
 public:
  // A JobDelegate is instantiated for each worker task that is run.
  // |task_source| is the task source whose worker task is running with this
  // delegate and |pooled_task_runner_delegate| is used by ShouldYield() to
  // check whether the pool wants this worker task to yield (null if this worker
  // should never yield -- e.g. when the main thread is a worker).
  JobDelegate(internal::JobTaskSource* task_source,
              internal::PooledTaskRunnerDelegate* pooled_task_runner_delegate);
  ~JobDelegate();

  // Returns true if this thread should return from the worker task on the
  // current thread ASAP. Workers should periodically invoke ShouldYield (or
  // YieldIfNeeded()) as often as is reasonable.
  bool ShouldYield();

  // If ShouldYield(), this will pause the current thread (allowing it to be
  // replaced in the pool); no-ops otherwise. If it pauses, it will resume and
  // return from this call whenever higher priority work completes.
  // Prefer ShouldYield() over this (only use YieldIfNeeded() when unwinding
  // the stack is not possible).
  void YieldIfNeeded();

  // Notifies the scheduler that max concurrency was increased, and the number
  // of worker should be adjusted.
  void NotifyConcurrencyIncrease();

 private:
  // Verifies that either max concurrency is lower or equal to
  // |expected_max_concurrency|, or there is an increase version update
  // triggered by NotifyConcurrencyIncrease().
  void AssertExpectedConcurrency(size_t expected_max_concurrency);

  internal::JobTaskSource* const task_source_;
  internal::PooledTaskRunnerDelegate* const pooled_task_runner_delegate_;

#if DCHECK_IS_ON()
  // Used in AssertExpectedConcurrency(), see that method's impl for details.
  // Value of max concurrency recorded before running the worker task.
  size_t recorded_max_concurrency_;
  // Value of the increase version recorded before running the worker task.
  size_t recorded_increase_version_;
  // Value returned by the last call to ShouldYield().
  bool last_should_yield_ = false;
#endif

  DISALLOW_COPY_AND_ASSIGN(JobDelegate);
};

// Handle returned when posting a Job. Provides methods to control execution of
// the posted Job.
class BASE_EXPORT JobHandle {
 public:
  // A job must either be joined, canceled or detached before the JobHandle is
  // destroyed.
  ~JobHandle();

  JobHandle(JobHandle&&);
  JobHandle& operator=(JobHandle&&);

  // Update this Job's priority.
  void UpdatePriority(TaskPriority new_priority);

  // Notifies the scheduler that max concurrency was increased, and the number
  // of worker should be adjusted accordingly. This *must* be invoked shortly
  // after |max_concurrency_callback| starts returning a value larger than
  // previously returned values. This usually happens when new work items are
  // added and the API user wants additional |worker_task| to run concurrently.
  // TODO(839091): Update comment to be more specific about
  // |max_concurrency_callback| once PostJob() landed.
  void NotifyConcurrencyIncrease();

  // Contributes to the job on this thread. Doesn't return until all tasks have
  // completed and max concurrency becomes 0. This also promotes this Job's
  // priority to be at least as high as the calling thread's priority.
  void Join();

  // Forces all existing workers to yield ASAP. Waits until they have all
  // returned from the Job's callback before returning.
  void Cancel();

  // Forces all existing workers to yield ASAP but doesn’t wait for them.
  // Warning, this is dangerous if the Job's callback is bound to or has access
  // to state which may be deleted after this call.
  void CancelAndDetach();

  // Can be invoked before ~JobHandle() to avoid waiting on the job completing.
  void Detach();

 private:
  // TODO(839091): Update friendship once PostJob is used.
  friend class internal::JobTaskSource;

  explicit JobHandle(scoped_refptr<internal::JobTaskSource> task_source);

  scoped_refptr<internal::JobTaskSource> task_source_;

  DISALLOW_COPY_AND_ASSIGN(JobHandle);
};

}  // namespace experimental
}  // namespace base

#endif  // BASE_TASK_POST_JOB_H_