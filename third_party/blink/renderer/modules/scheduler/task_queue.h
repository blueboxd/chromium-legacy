// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_TASK_QUEUE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_TASK_QUEUE_H_

#include <memory>

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace blink {

class Document;
class ExecutionContext;
class Scheduler;
class ScriptValue;
class Task;
class TaskQueuePostTaskOptions;
class V8Function;
class WebSchedulingTaskQueue;

namespace scheduler {
class WebSchedulingTaskQueue;
}  // namespace scheduler

class MODULES_EXPORT TaskQueue : public ScriptWrappable,
                                 ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(TaskQueue);

 public:
  TaskQueue(Document*, WebSchedulingPriority, Scheduler*);

  // Returns the priority of the TaskQueue.
  AtomicString priority() const;

  // postTask creates and queues a Task in this TaskQueue and returns the Task.
  // If the underlying context is destroyed, e.g. for detached documents, this
  // returns nullptr.
  Task* postTask(V8Function*,
                 TaskQueuePostTaskOptions*,
                 const HeapVector<ScriptValue>& args);

  // Move the task from its current TaskQueue to this one. For pending
  // non-delayed tasks, the task is enqueued at the end of this TaskQueue. For
  // delayed tasks, the delay is adjusted before reposting it.
  void take(Task*);

  base::SingleThreadTaskRunner* GetTaskRunner() { return task_runner_.get(); }
  Scheduler* GetScheduler() { return scheduler_.Get(); }

  void ContextDestroyed(ExecutionContext*) override;

  void Trace(Visitor*) override;

 private:
  void RunTaskCallback(Task*);
  void ScheduleTask(Task*, base::TimeDelta delay);

  const WebSchedulingPriority priority_;
  std::unique_ptr<WebSchedulingTaskQueue> web_scheduling_task_queue_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  Member<Scheduler> scheduler_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_TASK_QUEUE_H_
