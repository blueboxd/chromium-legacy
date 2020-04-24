// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/dom_task_controller.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/scheduler/dom_task_signal.h"

namespace blink {

// static
DOMTaskController* DOMTaskController::Create(ExecutionContext* context,
                                             const AtomicString& priority) {
  return MakeGarbageCollected<DOMTaskController>(
      context, WebSchedulingPriorityFromString(priority));
}

DOMTaskController::DOMTaskController(ExecutionContext* context,
                                     WebSchedulingPriority priority)
    : AbortController(MakeGarbageCollected<DOMTaskSignal>(
          context,
          priority,
          DOMTaskSignal::Type::kCreatedByController)) {
  DCHECK(!context->IsContextDestroyed());
}

void DOMTaskController::setPriority(const AtomicString& priority) {
  GetTaskSignal()->SignalPriorityChange(
      WebSchedulingPriorityFromString(priority));
}

DOMTaskSignal* DOMTaskController::GetTaskSignal() const {
  return static_cast<DOMTaskSignal*>(signal());
}

}  // namespace blink
