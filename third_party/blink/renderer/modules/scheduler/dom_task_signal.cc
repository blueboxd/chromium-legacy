// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/dom_task_signal.h"

#include <utility>

#include "base/callback.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/modules/scheduler/dom_scheduler.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_task_queue.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

DOMTaskSignal::DOMTaskSignal(Document* document, WebSchedulingPriority priority)
    : AbortSignal(document),
      ContextLifecycleObserver(document),
      priority_(priority),
      web_scheduling_task_queue_(document->GetScheduler()
                                     ->ToFrameScheduler()
                                     ->CreateWebSchedulingTaskQueue(priority)) {
}

DOMTaskSignal::~DOMTaskSignal() = default;

AtomicString DOMTaskSignal::priority() {
  return WebSchedulingPriorityToString(priority_);
}

void DOMTaskSignal::ContextDestroyed(ExecutionContext*) {
  web_scheduling_task_queue_.reset();
}

void DOMTaskSignal::SignalPriorityChange(WebSchedulingPriority priority) {
  if (priority_ == priority)
    return;
  priority_ = priority;
  if (web_scheduling_task_queue_)
    web_scheduling_task_queue_->SetPriority(priority);
}

base::SingleThreadTaskRunner* DOMTaskSignal::GetTaskRunner() {
  return web_scheduling_task_queue_
             ? web_scheduling_task_queue_->GetTaskRunner().get()
             : nullptr;
}

void DOMTaskSignal::Trace(Visitor* visitor) {
  AbortSignal::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
