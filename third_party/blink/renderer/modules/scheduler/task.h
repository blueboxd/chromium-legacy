// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_TASK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_TASK_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class ScriptValue;
class TaskQueue;
class V8Function;

class MODULES_EXPORT Task : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit Task(TaskQueue*, V8Function*, const Vector<ScriptValue>& args);

  // Task IDL Interface.
  AtomicString priority() const;
  AtomicString status() const;
  void cancel();

  // Set the TaskHandle associated with this task, which is used for
  // cancellation.
  void SetTaskHandle(TaskHandle&&);

  // Invoke the |callback_|.
  void Invoke();

  // Returns true if the Task is pending, i.e. queued and runnable.
  bool IsPending() const;

  // Cancel a pending task. This will update the |status_| to kCanceled, and the
  // task will no longer be runnable.
  void CancelPendingTask();

  void Trace(Visitor*) override;

 private:
  // The Status associated with the task lifecycle. Tasks are "pending" before
  // they run, transition to "running" during execution, and end in "completed"
  // when execution completes.
  enum class Status {
    kPending,
    kRunning,
    kCanceled,
    kCompleted,
  };

  void SetTaskStatus(Status);

  static bool IsValidStatusChange(Status from, Status to);
  static AtomicString TaskStatusToString(Status);

  Status status_;
  TaskHandle task_handle_;
  Member<TaskQueue> task_queue_;
  Member<V8Function> callback_;
  Vector<ScriptValue> arguments_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_TASK_H_
