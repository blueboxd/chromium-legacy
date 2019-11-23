// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_TASK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_TASK_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"

namespace blink {
class AbortSignal;
class DOMScheduler;
class ScriptState;
class ScriptValue;
class V8Function;

// DOMTask represents a task scheduled via the web scheduling API. It will
// keep itself alive until DOMTask::Invoke is called, which may be after the
// callback's v8 context is invalid, in which case, the task will not be run.
class DOMTask final : public GarbageCollected<DOMTask> {
 public:
  DOMTask(DOMScheduler*,
          ScriptPromiseResolver*,
          V8Function*,
          const HeapVector<ScriptValue>& args,
          base::SingleThreadTaskRunner*,
          AbortSignal*,
          base::TimeDelta delay);

  virtual void Trace(Visitor*);

 private:
  // Entry point for running this DOMTask's |callback_|.
  void Invoke();
  // Internal step of Invoke that handles invoking the callback, including
  // catching any errors and retrieving the result.
  void InvokeInternal(ScriptState*);
  void Abort();

  Member<DOMScheduler> scheduler_;
  TaskHandle task_handle_;
  Member<V8Function> callback_;
  HeapVector<ScriptValue> arguments_;
  Member<ScriptPromiseResolver> resolver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_TASK_H_
