// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/job_task_source.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/task_features.h"
#include "base/task/thread_pool/pooled_task_runner_delegate.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/time/time_override.h"

namespace base {
namespace internal {

JobTaskSource::State::State() = default;
JobTaskSource::State::~State() = default;

JobTaskSource::State::Value JobTaskSource::State::Cancel() {
  return {value_.fetch_or(kCanceledMask, std::memory_order_relaxed)};
}

JobTaskSource::State::Value
JobTaskSource::State::TryIncrementWorkerCountRelease(size_t max_concurrency) {
  uint32_t value_before_add = value_.load(std::memory_order_relaxed);

  // std::memory_order_release on success to establish Release-Acquire ordering
  // with DecrementWorkerCountAcquire() (see WillRunTask()).
  while (!(value_before_add & kCanceledMask) &&
         (value_before_add >> kWorkerCountBitOffset) < max_concurrency &&
         !value_.compare_exchange_weak(
             value_before_add, value_before_add + kWorkerCountIncrement,
             std::memory_order_release, std::memory_order_relaxed)) {
  }
  return {value_before_add};
}

JobTaskSource::State::Value
JobTaskSource::State::DecrementWorkerCountAcquire() {
  const size_t value_before_sub = value_.fetch_sub(kWorkerCountIncrement);
  DCHECK((value_before_sub >> kWorkerCountBitOffset) > 0);
  return {value_before_sub};
}

JobTaskSource::State::Value JobTaskSource::State::Load() const {
  return {value_.load(std::memory_order_relaxed)};
}

JobTaskSource::JobTaskSource(
    const Location& from_here,
    const TaskTraits& traits,
    RepeatingCallback<void(experimental::JobDelegate*)> worker_task,
    RepeatingCallback<size_t()> max_concurrency_callback,
    PooledTaskRunnerDelegate* delegate)
    : TaskSource(traits, nullptr, TaskSourceExecutionMode::kJob),
      from_here_(from_here),
      max_concurrency_callback_(std::move(max_concurrency_callback)),
      worker_task_(base::BindRepeating(
          [](JobTaskSource* self,
             const RepeatingCallback<void(experimental::JobDelegate*)>&
                 worker_task) {
            // Each worker task has its own delegate with associated state.
            experimental::JobDelegate job_delegate{self, self->delegate_};
            worker_task.Run(&job_delegate);
          },
          base::Unretained(this),
          std::move(worker_task))),
      queue_time_(TimeTicks::Now()),
      delegate_(delegate) {
  DCHECK(delegate_);
}

JobTaskSource::~JobTaskSource() {
  // Make sure there's no outstanding active run operation left.
  DCHECK_EQ(state_.Load().worker_count(), 0U);
}

ExecutionEnvironment JobTaskSource::GetExecutionEnvironment() {
  return {SequenceToken::Create(), nullptr};
}

void JobTaskSource::Cancel(TaskSource::Transaction* transaction) {
  // Sets the kCanceledMask bit on |state_| so that further calls to
  // WillRunTask() never succeed. std::memory_order_relaxed is sufficient
  // because this task source never needs to be re-enqueued after Cancel().
  state_.Cancel();
}

TaskSource::RunStatus JobTaskSource::WillRunTask() {
  // When this call is caused by an increase of max concurrency followed by an
  // associated NotifyConcurrencyIncrease(), the priority queue lock guarantees
  // an happens-after relation with NotifyConcurrencyIncrease(). The memory
  // operations on |state| below and in DidProcessTask() use
  // std::memory_order_release and std::memory_order_acquire respectively to
  // establish a Release-Acquire ordering. This ensures that all memory
  // side-effects made before this point, including an increase of max
  // concurrency followed by NotifyConcurrencyIncrease() are visible to a
  // DidProcessTask() call which is ordered after this one.
  const size_t max_concurrency = GetMaxConcurrency();
  const auto state_before_add =
      state_.TryIncrementWorkerCountRelease(max_concurrency);

  // Don't allow this worker to run the task if either:
  //   A) |state_| was canceled.
  //   B) |worker_count| is already at |max_concurrency|.
  //   C) |max_concurrency| was lowered below or to |worker_count|.
  // Case A:
  if (state_before_add.is_canceled())
    return RunStatus::kDisallowed;
  const size_t worker_count_before_add = state_before_add.worker_count();
  // Case B) or C):
  if (worker_count_before_add >= max_concurrency)
    return RunStatus::kDisallowed;

  DCHECK_LT(worker_count_before_add, max_concurrency);
  return max_concurrency == worker_count_before_add + 1
             ? RunStatus::kAllowedSaturated
             : RunStatus::kAllowedNotSaturated;
}

size_t JobTaskSource::GetRemainingConcurrency() const {
  // std::memory_order_relaxed is sufficient because no other state is
  // synchronized with GetRemainingConcurrency().
  const auto state = state_.Load();
  const size_t max_concurrency = GetMaxConcurrency();
  // Avoid underflows.
  if (state.is_canceled() || state.worker_count() > max_concurrency)
    return 0;
  return max_concurrency - state.worker_count();
}

void JobTaskSource::NotifyConcurrencyIncrease() {
#if DCHECK_IS_ON()
  {
    AutoLock auto_lock(version_lock_);
    ++increase_version_;
    version_condition_.Broadcast();
  }
#endif  // DCHECK_IS_ON()
  // Make sure the task source is in the queue if not already.
  // Caveat: it's possible but unlikely that the task source has already reached
  // its intended concurrency and doesn't need to be enqueued if there
  // previously were too many worker. For simplicity, the task source is always
  // enqueued and will get discarded if already saturated when it is popped from
  // the priority queue.
  delegate_->EnqueueJobTaskSource(this);
}

size_t JobTaskSource::GetMaxConcurrency() const {
  return max_concurrency_callback_.Run();
}

bool JobTaskSource::ShouldYield() const {
  return state_.Load().is_canceled();
}

#if DCHECK_IS_ON()

size_t JobTaskSource::GetConcurrencyIncreaseVersion() const {
  AutoLock auto_lock(version_lock_);
  return increase_version_;
}

bool JobTaskSource::WaitForConcurrencyIncreaseUpdate(size_t recorded_version) {
  AutoLock auto_lock(version_lock_);
  constexpr TimeDelta timeout = TimeDelta::FromSeconds(1);
  const base::TimeTicks start_time = subtle::TimeTicksNowIgnoringOverride();
  do {
    DCHECK_LE(recorded_version, increase_version_);
    if (recorded_version != increase_version_)
      return true;
    // Waiting is acceptable because it is in DCHECK-only code.
    ScopedAllowBaseSyncPrimitivesOutsideBlockingScope
        allow_base_sync_primitives;
    version_condition_.TimedWait(timeout);
  } while (subtle::TimeTicksNowIgnoringOverride() - start_time < timeout);
  return false;
}

#endif  // DCHECK_IS_ON()

Optional<Task> JobTaskSource::TakeTask(TaskSource::Transaction* transaction) {
  // JobTaskSource members are not lock-protected so no need to acquire a lock
  // if |transaction| is nullptr.
  DCHECK_GT(state_.Load().worker_count(), 0U);
  DCHECK(worker_task_);
  return base::make_optional<Task>(from_here_, worker_task_, TimeDelta());
}

bool JobTaskSource::DidProcessTask(TaskSource::Transaction* transaction) {
  // std::memory_order_acquire on |state_| is necessary to establish
  // Release-Acquire ordering (see WillRunTask()).
  // When the task source needs to be queued, either because the current task
  // yielded or because of NotifyConcurrencyIncrease(), one of the following is
  // true:
  //   A) The JobTaskSource is already in the queue (no worker picked up the
  //      extra work yet): Incorrectly returning false is fine and the memory
  //      barrier may be ineffective.
  //   B) The JobTaskSource() is no longer in the queue: The Release-Acquire
  //      ordering with WillRunTask() established by |state_| ensures that
  //      the upcoming call for GetMaxConcurrency() happens-after any
  //      NotifyConcurrencyIncrease() that happened-before WillRunTask(). If
  //      this task completed because it yielded, this barrier guarantees that
  //      it sees an up-to-date concurrency value and correctly re-enqueues.
  //
  // Note that stale values the other way around (incorrectly re-enqueuing) are
  // not an issue because the queues support empty task sources.
  const auto state_before_sub = state_.DecrementWorkerCountAcquire();

  // A canceled task source should never get re-enqueued.
  if (state_before_sub.is_canceled())
    return false;

  DCHECK_GT(state_before_sub.worker_count(), 0U);

  // Re-enqueue the TaskSource if the task ran and the worker count is below the
  // max concurrency.
  return state_before_sub.worker_count() <= GetMaxConcurrency();
}

SequenceSortKey JobTaskSource::GetSortKey() const {
  return SequenceSortKey(traits_.priority(), queue_time_);
}

Optional<Task> JobTaskSource::Clear(TaskSource::Transaction* transaction) {
  Cancel();
  // Nothing is cleared since other workers might still racily run tasks. For
  // simplicity, the destructor will take care of it once all references are
  // released.
  return base::make_optional<Task>(from_here_, DoNothing(), TimeDelta());
}

}  // namespace internal
}  // namespace base
