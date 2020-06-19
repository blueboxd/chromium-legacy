// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/agent_scheduling_strategy.h"

#include <algorithm>
#include <memory>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/common/pollable_thread_safe_flag.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"

namespace blink {
namespace scheduler {
namespace {

using ::base::sequence_manager::TaskQueue;

// Scheduling strategy that does nothing. This emulates the "current" shipped
// behavior, and is the default unless overridden. Corresponds to the
// |kNoOpStrategy| feature.
class NoOpStrategy final : public AgentSchedulingStrategy {
 public:
  NoOpStrategy() = default;

  ShouldUpdatePolicy OnFrameAdded(const FrameSchedulerImpl&) override {
    VerifyValidSequence();
    return ShouldUpdatePolicy::kNo;
  }
  ShouldUpdatePolicy OnFrameRemoved(const FrameSchedulerImpl&) override {
    VerifyValidSequence();
    return ShouldUpdatePolicy::kNo;
  }
  ShouldUpdatePolicy OnMainFrameFirstMeaningfulPaint(
      const FrameSchedulerImpl&) override {
    VerifyValidSequence();
    return ShouldUpdatePolicy::kNo;
  }
  ShouldUpdatePolicy OnInputEvent() override {
    VerifyValidSequence();
    return ShouldUpdatePolicy::kNo;
  }
  ShouldUpdatePolicy OnDocumentChangedInMainFrame(
      const FrameSchedulerImpl& frame_scheduler) override {
    VerifyValidSequence();
    return ShouldUpdatePolicy::kNo;
  }

  base::Optional<bool> QueueEnabledState(
      const MainThreadTaskQueue& task_queue) const override {
    VerifyValidSequence();
    return base::nullopt;
  }
  base::Optional<TaskQueue::QueuePriority> QueuePriority(
      const MainThreadTaskQueue& task_queue) const override {
    VerifyValidSequence();
    return base::nullopt;
  }

  bool ShouldNotifyOnInputEvent() const override { return false; }
};

// Base class for strategies that keep track of main frame FMP for scheduling
// decisions.
class TrackMainFrameFMP : public AgentSchedulingStrategy {
 public:
  TrackMainFrameFMP() : waiting_for_input_(&waiting_for_input_lock_) {}

  ShouldUpdatePolicy OnFrameAdded(
      const FrameSchedulerImpl& frame_scheduler) final {
    VerifyValidSequence();
    return OnNewDocument(frame_scheduler);
  }

  ShouldUpdatePolicy OnFrameRemoved(
      const FrameSchedulerImpl& frame_scheduler) final {
    VerifyValidSequence();
    if (frame_scheduler.GetFrameType() !=
        FrameScheduler::FrameType::kMainFrame) {
      return ShouldUpdatePolicy::kNo;
    }

    main_frames_.erase(&frame_scheduler);
    main_frames_waiting_for_fmp_.erase(&frame_scheduler);
    if (main_frames_waiting_for_fmp_.IsEmpty())
      SetWaitingForInput(false);

    // TODO(talp): If the frame wasn't in the set to begin with (e.g.: because
    //  it already hit FMP), or if there are still other frames in the set,
    //  then we may not have to trigger a policy update. (But what about cases
    //  where the current agent just changed from main to non-main?)
    return ShouldUpdatePolicy::kYes;
  }

  ShouldUpdatePolicy OnMainFrameFirstMeaningfulPaint(
      const FrameSchedulerImpl& frame_scheduler) final {
    VerifyValidSequence();
    DCHECK(frame_scheduler.GetFrameType() ==
           FrameScheduler::FrameType::kMainFrame);

    main_frames_waiting_for_fmp_.erase(&frame_scheduler);
    if (main_frames_waiting_for_fmp_.IsEmpty())
      SetWaitingForInput(false);

    // TODO(talp): If the frame wasn't in the set to begin with (e.g.: because
    //  an input even cleared it), or if there are still other frames in the
    //  set, then we may not have to trigger a policy update.
    return ShouldUpdatePolicy::kYes;
  }

  ShouldUpdatePolicy OnInputEvent() final {
    VerifyValidSequence();
    if (main_frames_waiting_for_fmp_.IsEmpty())
      return ShouldUpdatePolicy::kNo;

    main_frames_waiting_for_fmp_.clear();
    SetWaitingForInput(false);
    return ShouldUpdatePolicy::kYes;
  }

  ShouldUpdatePolicy OnDocumentChangedInMainFrame(
      const FrameSchedulerImpl& frame_scheduler) final {
    VerifyValidSequence();
    return OnNewDocument(frame_scheduler);
  }

  bool ShouldNotifyOnInputEvent() const override {
    return waiting_for_input_.IsSet();
  }

 protected:
  ShouldUpdatePolicy OnNewDocument(const FrameSchedulerImpl& frame_scheduler) {
    // For now we *always* return kYes here. It might be possible to optimize
    // this, but there are a number of tricky cases that need to be taken into
    // account here: (i) a non-main frame could have navigated between a main
    // and a non-main agent, possibly requiring policy update for that frame, or
    // (ii) main frame navigated to a different agent, potentially changing the
    // main/non-main classification for both the "previous" and "current" agents
    // and requiring their policies be updated.

    if (frame_scheduler.GetFrameType() !=
        FrameScheduler::FrameType::kMainFrame) {
      return ShouldUpdatePolicy::kYes;
    }

    main_frames_.insert(&frame_scheduler);
    main_frames_waiting_for_fmp_.insert(&frame_scheduler);
    SetWaitingForInput(true);

    return ShouldUpdatePolicy::kYes;
  }

  bool ShouldAffectQueue(const FrameSchedulerImpl& frame) const {
    // Don't do anything if all main frames reached FMP.
    if (main_frames_waiting_for_fmp_.IsEmpty())
      return false;

    // Otherwise, affect the queue only if it doesn't belong to any main agent.
    base::UnguessableToken agent_cluster_id = frame.GetAgentClusterId();
    return std::all_of(
        main_frames_.begin(), main_frames_.end(),
        [agent_cluster_id](const FrameSchedulerImpl* frame_scheduler) {
          return frame_scheduler->GetAgentClusterId() != agent_cluster_id;
        });
  }

 private:
  WTF::HashSet<const FrameSchedulerImpl*> main_frames_;
  WTF::HashSet<const FrameSchedulerImpl*> main_frames_waiting_for_fmp_;

  base::Lock waiting_for_input_lock_;
  PollableThreadSafeFlag waiting_for_input_;
  void SetWaitingForInput(bool waiting_for_input) {
    if (waiting_for_input_.IsSet() != waiting_for_input) {
      base::AutoLock lock(waiting_for_input_lock_);
      waiting_for_input_.SetWhileLocked(waiting_for_input);
    }
  }
};

// Strategy that disables non-main agents' timer queues until all main frames
// have reached FMP. Corresponds to the |kDisableTimersStrategy| feature.
class DisableTimerQueuesUntilFMP final : public TrackMainFrameFMP {
 public:
  DisableTimerQueuesUntilFMP() = default;

  base::Optional<bool> QueueEnabledState(
      const MainThreadTaskQueue& task_queue) const override {
    VerifyValidSequence();

    // For now we only disable timer queues.
    if (task_queue.queue_class() != MainThreadTaskQueue::QueueClass::kTimer)
      return base::nullopt;

    if (!ShouldAffectQueue(*task_queue.GetFrameScheduler()))
      return base::nullopt;

    return false;
  }

  base::Optional<TaskQueue::QueuePriority> QueuePriority(
      const MainThreadTaskQueue& task_queue) const override {
    VerifyValidSequence();
    return base::nullopt;
  }
};

// Strategy that reduces non-main agents' timer queues' priority to
// |kBestEffortPriority| until all main frames have reached FMP. Corresponds to
// the |kBestEffortPriorityTimersStrategy| feature.
class BestEffortPriorityTimersUntilFMP final : public TrackMainFrameFMP {
 public:
  BestEffortPriorityTimersUntilFMP() = default;

  base::Optional<bool> QueueEnabledState(
      const MainThreadTaskQueue& task_queue) const override {
    VerifyValidSequence();
    return base::nullopt;
  }

  base::Optional<TaskQueue::QueuePriority> QueuePriority(
      const MainThreadTaskQueue& task_queue) const override {
    VerifyValidSequence();

    // For now we only disable timer queues.
    if (task_queue.queue_class() != MainThreadTaskQueue::QueueClass::kTimer)
      return base::nullopt;

    if (!ShouldAffectQueue(*task_queue.GetFrameScheduler()))
      return base::nullopt;

    return TaskQueue::QueuePriority::kBestEffortPriority;
  }
};

}  // namespace

AgentSchedulingStrategy::~AgentSchedulingStrategy() {
  VerifyValidSequence();
}

std::unique_ptr<AgentSchedulingStrategy> AgentSchedulingStrategy::Create() {
  if (!base::FeatureList::IsEnabled(kPerAgentSchedulingExperiments))
    return std::make_unique<NoOpStrategy>();

  switch (kPerAgentStrategy.Get()) {
    case PerAgentSchedulingStrategyExperiments::kNoOpStrategy:
      return std::make_unique<NoOpStrategy>();
    case PerAgentSchedulingStrategyExperiments::kDisableTimersStrategy:
      return std::make_unique<DisableTimerQueuesUntilFMP>();
    case PerAgentSchedulingStrategyExperiments::
        kBestEffortPriorityTimersStrategy:
      return std::make_unique<BestEffortPriorityTimersUntilFMP>();
  }
}

void AgentSchedulingStrategy::VerifyValidSequence() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
}

}  // namespace scheduler
}  // namespace blink
