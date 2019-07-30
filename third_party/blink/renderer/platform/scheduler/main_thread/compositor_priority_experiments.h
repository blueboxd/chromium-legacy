// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_COMPOSITOR_PRIORITY_EXPERIMENTS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_COMPOSITOR_PRIORITY_EXPERIMENTS_H_

#include "base/task/sequence_manager/task_queue.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/common/cancelable_closure_holder.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {
namespace scheduler {

using QueuePriority = base::sequence_manager::TaskQueue::QueuePriority;

class MainThreadSchedulerImpl;
class MainThreadTaskQueue;

class PLATFORM_EXPORT CompositorPriorityExperiments {
  DISALLOW_NEW();

 public:
  explicit CompositorPriorityExperiments(MainThreadSchedulerImpl* scheduler);
  ~CompositorPriorityExperiments();

  enum class Experiment {
    kNone,
    kVeryHighPriorityForCompositingAlways,
    kVeryHighPriorityForCompositingWhenFast,
    kVeryHighPriorityForCompositingAlternating,
    kVeryHighPriorityForCompositingAfterDelay
  };

  bool IsExperimentActive() const;

  QueuePriority GetCompositorPriority() const;

  void SetCompositingIsFast(bool compositing_is_fast);

  void OnTaskCompleted(MainThreadTaskQueue* queue,
                       QueuePriority current_priority);

  QueuePriority GetAlternatingPriority() const {
    return alternating_compositor_priority_;
  }

  void OnMainThreadSchedulerInitialized();

 private:
  MainThreadSchedulerImpl* scheduler_;  // Not owned.

  static Experiment GetExperimentFromFeatureList();

  void DoPrioritizeCompositingAfterDelay();

  void PostPrioritizeCompositingAfterDelayTask();

  const Experiment experiment_;

  QueuePriority alternating_compositor_priority_ =
      QueuePriority::kVeryHighPriority;

  bool compositing_is_fast_ = false;

  QueuePriority delay_compositor_priority_ = QueuePriority::kNormalPriority;

  CancelableClosureHolder do_prioritize_compositing_after_delay_callback_;

  base::TimeDelta prioritize_compositing_after_delay_length_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_COMPOSITOR_PRIORITY_EXPERIMENTS_H_
