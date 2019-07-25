// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/scheduler_sequence.h"
#include "gpu/command_buffer/service/scheduler.h"

namespace gpu {

SchedulerSequence::SchedulerSequence(Scheduler* scheduler)
    : SingleTaskSequence(),
      scheduler_(scheduler),
      sequence_id_(scheduler->CreateSequence(SchedulingPriority::kHigh)) {}

// Note: this drops tasks not executed yet.
SchedulerSequence::~SchedulerSequence() {
  scheduler_->DestroySequence(sequence_id_);
}

// SingleTaskSequence implementation.
SequenceId SchedulerSequence::GetSequenceId() {
  return sequence_id_;
}

bool SchedulerSequence::ShouldYield() {
  return scheduler_->ShouldYield(sequence_id_);
}

void SchedulerSequence::ScheduleTask(base::OnceClosure task,
                                     std::vector<SyncToken> sync_token_fences) {
  scheduler_->ScheduleTask(Scheduler::Task(sequence_id_, std::move(task),
                                           std::move(sync_token_fences)));
}

void SchedulerSequence::ContinueTask(base::OnceClosure task) {
  scheduler_->ContinueTask(sequence_id_, std::move(task));
}

}  // namespace gpu
