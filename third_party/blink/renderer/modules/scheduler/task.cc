// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/task.h"

#include <utility>

#include "base/logging.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_function.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/scheduler/scheduler.h"
#include "third_party/blink/renderer/modules/scheduler/task_queue.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

Task::Task(TaskQueue* task_queue,
           ExecutionContext* context,
           V8Function* callback,
           const HeapVector<ScriptValue>& args,
           base::TimeDelta delay)
    : ContextLifecycleObserver(context),
      status_(Status::kPending),
      task_queue_(task_queue),
      callback_(callback),
      arguments_(args),
      delay_(delay),
      queue_time_(delay.is_zero() ? base::TimeTicks()
                                  : base::TimeTicks::Now()) {
  DCHECK(task_queue_);
  DCHECK(callback_);

  Schedule(delay_);
}

void Task::Trace(Visitor* visitor) {
  visitor->Trace(task_queue_);
  visitor->Trace(callback_);
  visitor->Trace(arguments_);
  visitor->Trace(result_value_);
  visitor->Trace(result_promise_);
  visitor->Trace(exception_);
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

void Task::ContextDestroyed(ExecutionContext* context) {
  if (status_ != Status::kPending)
    return;
  CancelPendingTask();
}

AtomicString Task::priority() const {
  DCHECK(task_queue_);
  return task_queue_->priority();
}

AtomicString Task::status() const {
  return TaskStatusToString(status_);
}

void Task::cancel(ScriptState* script_state) {
  if (status_ != Status::kPending)
    return;
  CancelPendingTask();
  SetTaskStatus(Status::kCanceled);
  ResolveOrRejectPromiseIfNeeded(script_state);
}

ScriptPromise Task::result(ScriptState* script_state) {
  if (!result_promise_) {
    result_promise_ = MakeGarbageCollected<TaskResultPromise>(
        ExecutionContext::From(script_state), this,
        TaskResultPromise::kFinished);
    ResolveOrRejectPromiseIfNeeded(script_state);
  }
  return result_promise_->Promise(script_state->World());
}

void Task::MoveTo(TaskQueue* task_queue) {
  if (status_ != Status::kPending || task_queue == task_queue_)
    return;

  CancelPendingTask();
  task_queue_ = task_queue;

  base::TimeDelta delay = base::TimeDelta();
  if (!delay_.is_zero()) {
    DCHECK(!queue_time_.is_null());
    base::TimeTicks now = base::TimeTicks::Now();
    if (queue_time_ + delay_ > now) {
      delay = now - queue_time_;
    }
  }

  Schedule(delay);
}

void Task::Schedule(base::TimeDelta delay) {
  DCHECK_EQ(status_, Status::kPending);
  DCHECK(!task_handle_.IsActive());
  DCHECK_GE(delay, base::TimeDelta());

  task_handle_ = PostDelayedCancellableTask(
      *task_queue_->GetTaskRunner(), FROM_HERE,
      WTF::Bind(&Task::Invoke, WrapPersistent(this)), delay_);
}

void Task::CancelPendingTask() {
  DCHECK_EQ(status_, Status::kPending);
  DCHECK(task_handle_.IsActive());

  task_handle_.Cancel();
}

void Task::Invoke() {
  DCHECK_EQ(status_, Status::kPending);
  DCHECK(callback_);
  DCHECK(GetExecutionContext());
  DCHECK(!GetExecutionContext()->IsContextDestroyed());

  ScriptState* script_state =
      callback_->CallbackRelevantScriptStateOrReportError("Task", "Invoke");
  if (!script_state || !script_state->ContextIsValid())
    return;

  SetTaskStatus(Status::kRunning);
  task_queue_->GetScheduler()->OnTaskStarted(task_queue_, this);
  InvokeInternal(script_state);
  SetTaskStatus(Status::kCompleted);
  task_queue_->GetScheduler()->OnTaskCompleted(task_queue_, this);
  ResolveOrRejectPromiseIfNeeded(script_state);
  callback_.Release();
}

void Task::InvokeInternal(ScriptState* script_state) {
  ScriptState::Scope scope(script_state);
  v8::TryCatch try_catch(script_state->GetIsolate());
  try_catch.SetVerbose(true);

  ScriptValue result;
  if (!callback_->Invoke(nullptr, arguments_).To(&result)) {
    if (try_catch.HasCaught()) {
      exception_.Set(script_state->GetIsolate(), try_catch.Exception());
    }
    return;
  }
  result_value_.Set(script_state->GetIsolate(), result.V8Value());
}

void Task::ResolveOrRejectPromiseIfNeeded(ScriptState* script_state) {
  // Lazily resolove or reject the Promise - we don't do anything unless the
  // result property has been accessed.
  if (!result_promise_)
    return;

  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  if (!exception_.IsEmpty()) {
    result_promise_->Reject(ScriptValue::From(
        script_state, exception_.NewLocal(script_state->GetIsolate())));
    return;
  }

  // TODO(shaseley): Once we have continuation built, consider resolving this
  // promise async with continuation timing.
  if (status_ == Status::kCompleted) {
    result_promise_->Resolve(ScriptValue::From(
        script_state, result_value_.NewLocal(script_state->GetIsolate())));
    return;
  }

  if (status_ == Status::kCanceled) {
    result_promise_->Reject(ScriptValue::From(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError)));
    return;
  }
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
    // Canceled and Completed are both end states.
    case Status::kCanceled:
    case Status::kCompleted:
      return false;
  }
  NOTREACHED();
  return false;
}

}  // namespace blink
