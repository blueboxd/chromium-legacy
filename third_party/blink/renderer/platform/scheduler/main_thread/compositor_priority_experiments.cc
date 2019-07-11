// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/compositor_priority_experiments.h"

#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"

namespace blink {
namespace scheduler {

using QueuePriority = base::sequence_manager::TaskQueue::QueuePriority;

CompositorPriorityExperiments::CompositorPriorityExperiments(
    MainThreadSchedulerImpl* scheduler)
    : scheduler_(scheduler), experiment_(GetExperimentFromFeatureList()) {}

CompositorPriorityExperiments::~CompositorPriorityExperiments() {}

CompositorPriorityExperiments::Experiment
CompositorPriorityExperiments::GetExperimentFromFeatureList() {
  if (base::FeatureList::IsEnabled(kVeryHighPriorityForCompositingAlways)) {
    return Experiment::kVeryHighPriorityForCompositingAlways;
  } else if (base::FeatureList::IsEnabled(
                 kVeryHighPriorityForCompositingWhenFast)) {
    return Experiment::kVeryHighPriorityForCompositingWhenFast;
  } else if (base::FeatureList::IsEnabled(
                 kVeryHighPriorityForCompositingAlternating)) {
    return Experiment::kVeryHighPriorityForCompositingAlternating;
  } else {
    return Experiment::kNone;
  }
}

bool CompositorPriorityExperiments::IsExperimentActive() const {
  if (experiment_ == Experiment::kNone)
    return false;
  return true;
}

QueuePriority CompositorPriorityExperiments::GetCompositorPriority() const {
  switch (experiment_) {
    case Experiment::kVeryHighPriorityForCompositingAlways:
      return QueuePriority::kVeryHighPriority;
    case Experiment::kVeryHighPriorityForCompositingWhenFast:
      if (compositing_is_fast_) {
        return QueuePriority::kVeryHighPriority;
      }
      return QueuePriority::kNormalPriority;
    case Experiment::kVeryHighPriorityForCompositingAlternating:
      return alternating_compositor_priority_;
    case Experiment::kNone:
      NOTREACHED();
      return QueuePriority::kNormalPriority;
  }
}

void CompositorPriorityExperiments::SetCompositingIsFast(
    bool compositing_is_fast) {
  compositing_is_fast_ = compositing_is_fast;
}

void CompositorPriorityExperiments::OnTaskCompleted(
    MainThreadTaskQueue* queue,
    QueuePriority current_compositor_priority) {
  if (!queue)
    return;

  // Don't change priorities if compositor priority is already set to highest
  // or higher.
  if (current_compositor_priority <= QueuePriority::kHighestPriority)
    return;

  switch (experiment_) {
    case Experiment::kVeryHighPriorityForCompositingAlways:
      return;
    case Experiment::kVeryHighPriorityForCompositingWhenFast:
      return;
    case Experiment::kVeryHighPriorityForCompositingAlternating:
      // Deprioritize the compositor if it has just run a task. Prioritize the
      // compositor if another task has run regardless of its priority. This
      // prevents starving the compositor while allowing other work to run
      // in-between.
      if (queue->queue_type() == MainThreadTaskQueue::QueueType::kCompositor &&
          alternating_compositor_priority_ ==
              QueuePriority::kVeryHighPriority) {
        alternating_compositor_priority_ = QueuePriority::kNormalPriority;
        scheduler_->OnCompositorPriorityExperimentUpdateCompositorPriority();
      } else if (alternating_compositor_priority_ ==
                 QueuePriority::kNormalPriority) {
        alternating_compositor_priority_ = QueuePriority::kVeryHighPriority;
        scheduler_->OnCompositorPriorityExperimentUpdateCompositorPriority();
      }
      return;
    case Experiment::kNone:
      return;
  }
}

}  // namespace scheduler
}  // namespace blink
