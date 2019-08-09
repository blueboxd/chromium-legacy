// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/task.h"

#include <utility>

#include "base/logging.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_function.h"
#include "third_party/blink/renderer/modules/scheduler/task_queue.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

Task::Task(TaskQueue* task_queue,
           V8Function* callback,
           const Vector<ScriptValue>& args)
    : status_(Status::kPending),
      task_queue_(task_queue),
      callback_(callback),
      arguments_(args) {
  DCHECK(task_queue_);
  DCHECK(callback_);
}

void Task::Trace(Visitor* visitor) {
  visitor->Trace(task_queue_);
  visitor->Trace(callback_);
  ScriptWrappable::Trace(visitor);
}

AtomicString Task::priority() const {
  DCHECK(task_queue_);
  return task_queue_->priority();
}

AtomicString Task::status() const {
  return TaskStatusToString(status_);
}

void Task::cancel() {
  if (!IsPending())
    return;
  CancelPendingTask();
  SetTaskStatus(Status::kCanceled);
}

void Task::SetTaskHandle(TaskHandle&& task_handle) {
  task_handle_ = std::move(task_handle);
}

bool Task::IsPending() const {
  return status_ == Status::kPending;
}

void Task::CancelPendingTask() {
  DCHECK(IsPending());
  DCHECK(task_handle_.IsActive());

  task_handle_.Cancel();
}

void Task::Invoke() {
  DCHECK(IsPending());
  SetTaskStatus(Status::kRunning);
  callback_->InvokeAndReportException(nullptr, arguments_);
  SetTaskStatus(Status::kCompleted);
}

void Task::SetTaskStatus(Status status) {
  DCHECK(IsValidStatusChange(status_, status))
      << "Cannot transition from Task::Status " << TaskStatusToString(status_)
      << " to " << TaskStatusToString(status);
  status_ = status;
}

// static
AtomicString Task::TaskStatusToString(Status status) {
  DEFINE_STATIC_LOCAL(const AtomicString, pending, ("pending"));
  DEFINE_STATIC_LOCAL(const AtomicString, running, ("running"));
  DEFINE_STATIC_LOCAL(const AtomicString, canceled, ("canceled"));
  DEFINE_STATIC_LOCAL(const AtomicString, completed, ("completed"));

  switch (status) {
    case Status::kPending:
      return pending;
    case Status::kRunning:
      return running;
    case Status::kCanceled:
      return canceled;
    case Status::kCompleted:
      return completed;
  }

  NOTREACHED();
  return g_empty_atom;
}

// static
bool Task::IsValidStatusChange(Status from, Status to) {
  // Note: Self transitions are not valid.
  switch (from) {
    case Status::kPending: {
      switch (to) {
        // The task is invoked by the scheduler.
        case Status::kRunning:
        // task.cancel().
        case Status::kCanceled:
          return true;
        default:
          return false;
      }
    }
    case Status::kRunning: {
      switch (to) {
        // The task completes with or without exception.
        case Status::kCompleted:
          return true;
        default:
          return false;
      }
    }
    // Canceled and Finished are both end states.
    case Status::kCanceled:
    case Status::kCompleted:
      return false;
  }
  NOTREACHED();
  return false;
}

}  // namespace blink
