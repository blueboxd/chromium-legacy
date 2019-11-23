// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/dom_scheduler.h"

#include "base/memory/weak_ptr.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/scheduler/dom_task.h"
#include "third_party/blink/renderer/modules/scheduler/dom_task_signal.h"
#include "third_party/blink/renderer/modules/scheduler/scheduler_post_task_options.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_task_queue.h"

namespace blink {

namespace {

static ScriptPromise RejectPromiseImmediately(ScriptState* script_state) {
  return ScriptPromise::RejectWithDOMException(
      script_state,
      MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotSupportedError,
                                         "Current document is detached"));
}

}  // namespace

const char DOMScheduler::kSupplementName[] = "DOMScheduler";

DOMScheduler* DOMScheduler::From(Document& document) {
  DOMScheduler* scheduler = Supplement<Document>::From<DOMScheduler>(document);
  if (!scheduler) {
    scheduler = MakeGarbageCollected<DOMScheduler>(&document);
    Supplement<Document>::ProvideTo(document, scheduler);
  }
  return scheduler;
}

DOMScheduler::DOMScheduler(Document* document)
    : ContextLifecycleObserver(document) {
  if (document->IsContextDestroyed())
    return;
  DCHECK(document->GetScheduler());
  DCHECK(document->GetScheduler()->ToFrameScheduler());
  CreateGlobalTaskQueues(document);
}

void DOMScheduler::ContextDestroyed(ExecutionContext* context) {
  global_task_queues_.clear();
}

void DOMScheduler::Trace(Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  Supplement<Document>::Trace(visitor);
}

// TODO(japhet,shaseley): These are probably useful for metrics/tracing, or
// for handling incumbent task state.
void DOMScheduler::OnTaskStarted(DOMTask*) {}
void DOMScheduler::OnTaskCompleted(DOMTask*) {}

ScriptPromise DOMScheduler::postTask(ScriptState* script_state,
                                     V8Function* callback_function,
                                     SchedulerPostTaskOptions* options,
                                     const HeapVector<ScriptValue>& args) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed())
    return RejectPromiseImmediately(script_state);

  // Always honor the priority and the task signal if given. Therefore:
  // * If both priority and signal are set, use the signal but choose the
  //   default task runner for the given priority, so that it is scheduled at
  //   the given priority.
  // * If only the signal is set, use the signal and its associated priority.
  // * If only a priority is set, use the default task runner for that priority,
  //   and no signal.
  // * If neither is set, use the default priority task runner and no signal.
  //
  // Note that |options->signal()| may be a generic AbortSignal, rather than a
  // TaskSignal. In that case, use it for abort signalling, but use a default
  // task runner for priority purposes.
  base::SingleThreadTaskRunner* task_runner = nullptr;
  if (options->hasPriority()) {
    WebSchedulingPriority priority =
        WebSchedulingPriorityFromString(AtomicString(options->priority()));
    task_runner = GetTaskRunnerFor(priority);
  } else if (auto* task_signal = DynamicTo<DOMTaskSignal>(options->signal())) {
    task_runner = task_signal->GetTaskRunner();
    if (!task_runner)
      return RejectPromiseImmediately(script_state);
  } else {
    task_runner = GetTaskRunnerFor(WebSchedulingPriority::kDefaultPriority);
  }

  // TODO(shaseley): We need to figure out the behavior we want for delay. For
  // now, we use behavior that is very similar to setTimeout: negative delays
  // are treated as 0, and we use the Blink scheduler's delayed task behavior.
  // We don't, however, adjust the timeout based on nested calls (yet) or clamp
  // the value to a minimal delay.
  base::TimeDelta delay =
      base::TimeDelta::FromMilliseconds(std::max(0, options->delay()));

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  MakeGarbageCollected<DOMTask>(this, resolver, callback_function, args,
                                task_runner, options->signal(), delay);
  return resolver->Promise();
}

base::SingleThreadTaskRunner* DOMScheduler::GetTaskRunnerFor(
    WebSchedulingPriority priority) {
  DCHECK(!global_task_queues_.IsEmpty());
  return global_task_queues_[static_cast<int>(priority)]->GetTaskRunner().get();
}

void DOMScheduler::CreateGlobalTaskQueues(Document* document) {
  FrameScheduler* scheduler = document->GetScheduler()->ToFrameScheduler();
  for (size_t i = 0; i < kWebSchedulingPriorityCount; i++) {
    global_task_queues_.push_back(scheduler->CreateWebSchedulingTaskQueue(
        static_cast<WebSchedulingPriority>(i)));
  }
}

}  // namespace blink
