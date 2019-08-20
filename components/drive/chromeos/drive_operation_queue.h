// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_CHROMEOS_DRIVE_OPERATION_QUEUE_H_
#define COMPONENTS_DRIVE_CHROMEOS_DRIVE_OPERATION_QUEUE_H_

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/drive/file_errors.h"

namespace drive {
namespace internal {

// A rate limited queue for making calls to the drive backend. This is a basic
// token based queue that will loosely rate limit requests to the qps specified
// on construction. When enquing operations to an empty queue, the queue will
// operate in burst mode and rapidly schedule up to |desired_qps| operations.
template <typename T>
class DriveBackgroundOperationQueue {
 public:
  // |tick_clock| can be injected for testing.
  explicit DriveBackgroundOperationQueue(
      int desired_qps,
      const base::TickClock* tick_clock = base::DefaultTickClock::GetInstance())
      : tick_clock_(tick_clock),
        token_count_(desired_qps),
        per_token_time_delta_(
            base::TimeDelta::FromMilliseconds(1000 / desired_qps)),
        desired_qps_(desired_qps) {}

  ~DriveBackgroundOperationQueue() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  // Add a new operation to the queue. When the operation is scheduled to be
  // executed start_callback will be called. When the operation completes
  // finish_callback will be with the result.
  void AddOperation(
      base::WeakPtr<T> target,
      base::OnceCallback<void(const FileOperationCallback&)> start_callback,
      FileOperationCallback finish_callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(start_callback);
    DCHECK(finish_callback);

    drive_operation_queue_.emplace(std::move(target), std::move(start_callback),
                                   std::move(finish_callback));

    CheckAndMaybeStartTokenRefillTimer();
    CheckAndMaybeStartDriveOperation();
  }

  // Effectively disables the rate limiting for test purposes.
  void DisableQueueForTesting() {
    desired_qps_ = INT_MAX;
    token_count_ = INT_MAX;
  }

 private:
  struct DriveOperation;

  using OperationQueue = base::queue<DriveOperation>;
  using StartOperationCallback =
      base::OnceCallback<void(const FileOperationCallback&)>;
  using EndOperationCallback = FileOperationCallback;

  struct DriveOperation {
    DriveOperation(base::WeakPtr<T> target,
                   StartOperationCallback start_callback,
                   EndOperationCallback finish)
        : target(std::move(target)),
          start_callback(std::move(start_callback)),
          finish_callback(std::move(finish)) {}

    base::WeakPtr<T> target;
    StartOperationCallback start_callback;
    EndOperationCallback finish_callback;
  };

  void CheckAndMaybeStartDriveOperation() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    while (token_count_ > 0 && !drive_operation_queue_.empty()) {
      auto next = std::move(drive_operation_queue_.front());
      drive_operation_queue_.pop();

      if (!next.target) {
        continue;
      }
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(
              std::move(next.start_callback),
              base::BindRepeating(
                  &DriveBackgroundOperationQueue<T>::OnDriveOperationComplete,
                  weak_ptr_factory_.GetWeakPtr(),
                  std::move(next.finish_callback))));
      --token_count_;
    }
  }

  void OnDriveOperationComplete(FileOperationCallback callback,
                                FileError error) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    callback.Run(error);
  }

  void CheckAndMaybeStartTokenRefillTimer() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (token_refill_timer_.IsRunning()) {
      return;
    }

    base::TimeDelta elapsed_since_stopped =
        tick_clock_->NowTicks() - last_timer_stop_ticks_;
    int additional_tokens = elapsed_since_stopped / per_token_time_delta_;

    // Do not overflow when adding additional tokens.
    if (desired_qps_ - token_count_ < additional_tokens) {
      token_count_ = desired_qps_;
    } else {
      token_count_ += additional_tokens;
    }

    token_refill_timer_.Start(
        FROM_HERE, per_token_time_delta_,
        base::BindRepeating(&DriveBackgroundOperationQueue<T>::RefillTokens,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  void RefillTokens() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (token_count_ < desired_qps_) {
      ++token_count_;
    }
    CheckAndMaybeStartDriveOperation();

    if (drive_operation_queue_.empty()) {
      token_refill_timer_.Stop();
      last_timer_stop_ticks_ = tick_clock_->NowTicks();
    }
  }

  OperationQueue drive_operation_queue_;

  // Token Bucket timer, to refill tokens if required.
  const base::TickClock* tick_clock_;  // Not owned.
  base::RepeatingTimer token_refill_timer_;
  int token_count_;
  const base::TimeDelta per_token_time_delta_;
  int desired_qps_;
  base::TimeTicks last_timer_stop_ticks_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DriveBackgroundOperationQueue> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DriveBackgroundOperationQueue<T>);
};

}  // namespace internal
}  // namespace drive

#endif  // COMPONENTS_DRIVE_CHROMEOS_DRIVE_OPERATION_QUEUE_H_
