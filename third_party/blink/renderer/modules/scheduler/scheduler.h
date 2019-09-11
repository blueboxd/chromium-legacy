// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_SCHEDULER_H_

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class ExecutionContext;
class SchedulerPostTaskOptions;
class ScriptValue;
class Task;
class TaskQueue;
class V8Function;

// TODO(shaseley): Rename this class along with TaskQueue and Task to better
// differentiate these classes from the various other scheduler, task queue, and
// task classes in the codebase.
class MODULES_EXPORT Scheduler : public ScriptWrappable,
                                 public ContextLifecycleObserver,
                                 public Supplement<Document> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(Scheduler);

 public:
  static const char kSupplementName[];

  static Scheduler* From(Document&);

  explicit Scheduler(Document*);

  // Returns the TaskQueue of the currently executing Task. If the current task
  // was not scheduled through the scheduler, this returns the default priority
  // global task queue.
  TaskQueue* currentTaskQueue();

  // Returns the TaskQueue for the given |priority| or nullptr if the underlying
  // context is destroyed, e.g. for detached documents.
  TaskQueue* getTaskQueue(AtomicString priority);

  // postTask creates and queues a Task in the |global_task_queues_| entry
  // corresponding to the priority in the SchedulerPostTaskOptions, and returns
  // the Task. If the underlying context is destroyed, e.g. for detached
  // documents, this returns nullptr.
  Task* postTask(V8Function*,
                 SchedulerPostTaskOptions*,
                 const HeapVector<ScriptValue>& args);

  // Callbacks invoked by TaskQueues when they run scheduled tasks.
  void OnTaskStarted(TaskQueue*, Task*);
  void OnTaskCompleted(TaskQueue*, Task*);

  void ContextDestroyed(ExecutionContext*) override;

  void Trace(Visitor*) override;

 private:
  static constexpr size_t kWebSchedulingPriorityCount =
      static_cast<size_t>(WebSchedulingPriority::kLastPriority) + 1;

  void CreateGlobalTaskQueues(Document*);
  TaskQueue* GetTaskQueue(WebSchedulingPriority);

  // |global_task_queues_| is initialized with one entry per priority, indexed
  // by priority. This will be empty when the document is detached.
  HeapVector<Member<TaskQueue>, kWebSchedulingPriorityCount>
      global_task_queues_;
  // The TaskQueue associated with the currently running Task, or nullptr if the
  // current task was not scheduled through this Scheduler.
  Member<TaskQueue> current_task_queue_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_SCHEDULER_H_
