// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/dom_task.h"

#include <utility>

#include "base/logging.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_function.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/scheduler/dom_scheduler.h"
#include "third_party/blink/renderer/modules/scheduler/dom_task_signal.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

DOMTask::DOMTask(DOMScheduler* scheduler,
                 ScriptPromiseResolver* resolver,
                 V8Function* callback,
                 const HeapVector<ScriptValue>& args,
                 base::SingleThreadTaskRunner* task_runner,
                 AbortSignal* signal,
                 base::TimeDelta delay)
    : scheduler_(scheduler),
      callback_(callback),
      arguments_(args),
      resolver_(resolver) {
  DCHECK(callback_);
  DCHECK(task_runner);
  if (signal) {
    if (signal->aborted()) {
      Abort();
      return;
    }

    signal->AddAlgorithm(WTF::Bind(&DOMTask::Abort, WrapWeakPersistent(this)));
  }

  task_handle_ = PostDelayedCancellableTask(
      *task_runner, FROM_HERE,
      WTF::Bind(&DOMTask::Invoke, WrapPersistent(this)), delay);
}

void DOMTask::Trace(Visitor* visitor) {
  visitor->Trace(scheduler_);
  visitor->Trace(callback_);
  visitor->Trace(arguments_);
  visitor->Trace(resolver_);
}

void DOMTask::Invoke() {
  DCHECK(callback_);

  ScriptState* script_state =
      callback_->CallbackRelevantScriptStateOrReportError("DOMTask", "Invoke");
  if (!script_state || !script_state->ContextIsValid())
    return;

  scheduler_->OnTaskStarted(this);
  InvokeInternal(script_state);
  scheduler_->OnTaskCompleted(this);
  callback_.Release();
}

void DOMTask::InvokeInternal(ScriptState* script_state) {
  ScriptState::Scope scope(script_state);
  v8::TryCatch try_catch(script_state->GetIsolate());
  try_catch.SetVerbose(true);

  ScriptValue result;
  if (callback_->Invoke(nullptr, arguments_).To(&result))
    resolver_->Resolve(result.V8Value());
  else if (try_catch.HasCaught())
    resolver_->Reject(try_catch.Exception());
}

void DOMTask::Abort() {
  task_handle_.Cancel();
  resolver_->Reject(
      MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError));
}

}  // namespace blink
