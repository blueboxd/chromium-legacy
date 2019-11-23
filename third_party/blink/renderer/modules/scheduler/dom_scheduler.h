// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_SCHEDULER_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class DOMTask;
class ExecutionContext;
class SchedulerPostTaskOptions;
class ScriptValue;
class V8Function;
class WebSchedulingTaskQueue;

class MODULES_EXPORT DOMScheduler : public ScriptWrappable,
                                    public ContextLifecycleObserver,
                                    public Supplement<Document> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(DOMScheduler);

 public:
  static const char kSupplementName[];

  static DOMScheduler* From(Document&);

  explicit DOMScheduler(Document*);

  // postTask creates and queues a DOMTask and returns a Promise that will
  // resolve when it completes. The task will be scheduled in the queue
  // corresponding to the priority in the SchedulerPostTaskOptions, or in a
  // queue associated with the given DOMTaskSignal if one is provided. If the
  // underlying context is destroyed, e.g. for detached documents, this will
  // return a rejected promise.
  ScriptPromise postTask(ScriptState*,
                         V8Function*,
                         SchedulerPostTaskOptions*,
                         const HeapVector<ScriptValue>& args);

  // Callbacks invoked by DOMTasks when they run.
  void OnTaskStarted(DOMTask*);
  void OnTaskCompleted(DOMTask*);

  void ContextDestroyed(ExecutionContext*) override;

  void Trace(Visitor*) override;

 private:
  static constexpr size_t kWebSchedulingPriorityCount =
      static_cast<size_t>(WebSchedulingPriority::kLastPriority) + 1;

  void CreateGlobalTaskQueues(Document*);
  base::SingleThreadTaskRunner* GetTaskRunnerFor(WebSchedulingPriority);

  // |global_task_queues_| is initialized with one entry per priority, indexed
  // by priority. This will be empty when the document is detached.
  Vector<std::unique_ptr<WebSchedulingTaskQueue>, kWebSchedulingPriorityCount>
      global_task_queues_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_SCHEDULER_H_
