// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_TASK_SIGNAL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_TASK_SIGNAL_H_

#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
class Document;
class WebSchedulingTaskQueue;

class MODULES_EXPORT DOMTaskSignal final
    : public AbortSignal,
      public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(DOMTaskSignal);

 public:
  enum class Type { kCreatedByController, kImplicit };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class PriorityChangeStatus {
    kNoPriorityChange = 0,
    kPriorityHasChanged = 1,

    kMaxValue = kPriorityHasChanged
  };

  DOMTaskSignal(Document*, WebSchedulingPriority, Type);
  ~DOMTaskSignal() override;

  // task_signal.idl
  AtomicString priority();
  DEFINE_ATTRIBUTE_EVENT_LISTENER(prioritychange, kPrioritychange)

  void ContextDestroyed() override;

  void SignalPriorityChange(WebSchedulingPriority);
  base::SingleThreadTaskRunner* GetTaskRunner();

  bool IsTaskSignal() const override { return true; }

  void Trace(Visitor*) override;

  PriorityChangeStatus GetPriorityChangeStatus() const {
    return priority_change_status_;
  }

 private:
  WebSchedulingPriority priority_;

  // The lifetime of |web_scheduling_task_queue_| matches DOMTaskSignal, but
  // |web_scheduling_task_queue_| only holds a weak reference to the underlying
  // MainThreadTaskQueue. That weak reference will be invalidated on frame
  // detach, so a DOMTaskSignal will fail to schedule tasks in a detached
  // frame.
  std::unique_ptr<WebSchedulingTaskQueue> web_scheduling_task_queue_;

  PriorityChangeStatus priority_change_status_ =
      PriorityChangeStatus::kNoPriorityChange;
};

template <>
struct DowncastTraits<DOMTaskSignal> {
  static bool AllowFrom(const AbortSignal& signal) {
    return signal.IsTaskSignal();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_TASK_SIGNAL_H_
