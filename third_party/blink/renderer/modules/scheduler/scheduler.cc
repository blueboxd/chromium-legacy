// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/scheduler.h"

#include "base/memory/weak_ptr.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/scheduler/scheduler_post_task_options.h"
#include "third_party/blink/renderer/modules/scheduler/task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"

namespace blink {

const char Scheduler::kSupplementName[] = "Scheduler";

Scheduler* Scheduler::From(Document& document) {
  Scheduler* scheduler = Supplement<Document>::From<Scheduler>(document);
  if (!scheduler) {
    scheduler = MakeGarbageCollected<Scheduler>(&document);
    Supplement<Document>::ProvideTo(document, scheduler);
  }
  return scheduler;
}

Scheduler::Scheduler(Document* document) : ContextLifecycleObserver(document) {
  if (document->IsContextDestroyed())
    return;
  DCHECK(document->GetScheduler());
  DCHECK(document->GetScheduler()->ToFrameScheduler());
  CreateGlobalTaskQueues(document);
}

void Scheduler::ContextDestroyed(ExecutionContext* context) {
  global_task_queues_.clear();
}

void Scheduler::Trace(Visitor* visitor) {
  visitor->Trace(global_task_queues_);
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  Supplement<Document>::Trace(visitor);
}

TaskQueue* Scheduler::getTaskQueue(AtomicString priority) const {
  if (global_task_queues_.IsEmpty())
    return nullptr;
  return global_task_queues_[static_cast<int>(
      TaskQueue::WebSchedulingPriorityFromString(priority))];
}

Task* Scheduler::postTask(V8Function* callback_function,
                          SchedulerPostTaskOptions* options,
                          const Vector<ScriptValue>& args) {
  TaskQueue* task_queue = getTaskQueue(AtomicString(options->priority()));
  if (!task_queue)
    return nullptr;
  return task_queue->postTask(callback_function, options, args);
}

void Scheduler::CreateGlobalTaskQueues(Document* document) {
  for (size_t i = 0; i < kWebSchedulingPriorityCount; i++) {
    global_task_queues_.push_back(MakeGarbageCollected<TaskQueue>(
        document, static_cast<WebSchedulingPriority>(i)));
  }
}

}  // namespace blink
