// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/dom_scheduler.h"

#include "base/memory/weak_ptr.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_scheduler_post_task_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_task_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/scheduler/dom_task.h"
#include "third_party/blink/renderer/modules/scheduler/dom_task_signal.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_task_queue.h"

namespace blink {

namespace {

static ScriptPromise RejectPromiseImmediately(ExceptionState& exception_state) {
  // The bindings layer implicitly converts thrown exceptions in
  // promise-returning functions to promise rejections.
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Current document is detached");
  return ScriptPromise();
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
    : ExecutionContextLifecycleObserver(document),
      Supplement<Document>(*document) {
  if (document->IsContextDestroyed())
    return;
  DCHECK(document->GetScheduler());
  DCHECK(document->GetScheduler()->ToFrameScheduler());
  CreateGlobalTaskQueues(document);
}

void DOMScheduler::ContextDestroyed() {
  global_task_queues_.clear();
}

void DOMScheduler::Trace(Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  Supplement<Document>::Trace(visitor);
}

ScriptPromise DOMScheduler::postTask(ScriptState* script_state,
                                     V8Function* callback_function,
                                     SchedulerPostTaskOptions* options,
                                     const HeapVector<ScriptValue>& args,
                                     ExceptionState& exception_state) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed())
    return RejectPromiseImmediately(exception_state);
  if (options->signal() && options->signal()->aborted())
    return RejectPromiseImmediately(exception_state);

  // Always honor the priority and the task signal if given.
  DOMTaskSignal* task_signal = nullptr;
  if (!options->hasPriority() && IsA<DOMTaskSignal>(options->signal())) {
    // If only a signal is given, and it is a TaskSignal rather than an
    // basic AbortSignal, use it.
    task_signal = To<DOMTaskSignal>(options->signal());
  } else {
    // Otherwise, construct an implicit TaskSignal. Have it follow the signal
    // if it was given, so that it can still honor any aborts, but have it
    // at the fixed given priority (or default if none was specified).
    //
    // An implicit TaskSignal, in addition to being read-only, won't own its
    // own task queue. Instead, it will use the appropriate task queue from
    // |global_task_queues_|.
    WebSchedulingPriority priority =
        options->hasPriority()
            ? WebSchedulingPriorityFromString(AtomicString(options->priority()))
            : WebSchedulingPriority::kUserVisiblePriority;
    task_signal = MakeGarbageCollected<DOMTaskSignal>(
        GetSupplementable(), priority, DOMTaskSignal::Type::kImplicit);
    if (options->signal())
      task_signal->Follow(options->signal());
  }

  DCHECK(task_signal);
  if (!task_signal->GetTaskRunner())
    return RejectPromiseImmediately(exception_state);

  // TODO(shaseley): We need to figure out the behavior we want for delay. For
  // now, we use behavior that is very similar to setTimeout: negative delays
  // are treated as 0, and we use the Blink scheduler's delayed task behavior.
  // We don't, however, adjust the timeout based on nested calls (yet) or clamp
  // the value to a minimal delay.
  base::TimeDelta delay =
      base::TimeDelta::FromMilliseconds(std::max(0, options->delay()));

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  MakeGarbageCollected<DOMTask>(this, resolver, callback_function, args,
                                task_signal, delay);
  return resolver->Promise();
}

DOMTaskSignal* DOMScheduler::currentTaskSignal(
    ScriptState* script_state) const {
  v8::Local<v8::Value> embedder_data =
      script_state->GetContext()->GetContinuationPreservedEmbedderData();
  if (V8TaskSignal::HasInstance(embedder_data, script_state->GetIsolate()))
    return V8TaskSignal::ToImpl(v8::Local<v8::Object>::Cast(embedder_data));

  return MakeGarbageCollected<DOMTaskSignal>(
      GetSupplementable(), WebSchedulingPriority::kUserVisiblePriority,
      DOMTaskSignal::Type::kImplicit);
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
