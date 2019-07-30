// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_POST_JOB_H_
#define BASE_TASK_POST_JOB_H_

#include "base/base_export.h"
#include "base/macros.h"
#include "base/time/time.h"

namespace base {
namespace internal {
class JobTaskSource;
}
namespace experimental {

// Delegate that's passed to Job's worker task, providing an entry point to
// communicate with the scheduler.
class BASE_EXPORT JobDelegate {
 public:
  explicit JobDelegate(internal::JobTaskSource* task_source);

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
  internal::JobTaskSource* const task_source_;
};

}  // namespace experimental
}  // namespace base

#endif  // BASE_TASK_POST_JOB_H_