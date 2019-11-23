// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_TASK_SIGNAL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_TASK_SIGNAL_H_

#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
class Document;
class ExecutionContext;
class WebSchedulingTaskQueue;

class MODULES_EXPORT DOMTaskSignal final : public AbortSignal,
                                           public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(DOMTaskSignal);

 public:
  explicit DOMTaskSignal(Document*, WebSchedulingPriority);
  ~DOMTaskSignal() override;

  // task_signal.idl
  AtomicString priority();

  void ContextDestroyed(ExecutionContext*) override;

  void SignalPriorityChange(WebSchedulingPriority);
  base::SingleThreadTaskRunner* GetTaskRunner();

  bool IsTaskSignal() const override { return true; }

  void Trace(Visitor*) override;

 private:
  WebSchedulingPriority priority_;

  // The lifetime of |web_scheduling_task_queue_| matches DOMTaskSignal, but
  // |web_scheduling_task_queue_| only holds a weak reference to the underlying
  // MainThreadTaskQueue. That weak reference will be invalidated on frame
  // detach, so a DOMTaskSignal will fail to schedule tasks in a detached
  // frame.
  std::unique_ptr<WebSchedulingTaskQueue> web_scheduling_task_queue_;
};

template <>
struct DowncastTraits<DOMTaskSignal> {
  static bool AllowFrom(const AbortSignal& signal) {
    return signal.IsTaskSignal();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_TASK_SIGNAL_H_
