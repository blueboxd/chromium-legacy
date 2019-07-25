// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SCHEDULER_SEQUENCE_H_
#define GPU_IPC_SCHEDULER_SEQUENCE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/ipc/gl_in_process_context_export.h"
#include "gpu/ipc/single_task_sequence.h"

namespace gpu {
class Scheduler;

// SingleTaskSequence implementation that uses gpu scheduler sequences.
class GL_IN_PROCESS_CONTEXT_EXPORT SchedulerSequence
    : public SingleTaskSequence {
 public:
  explicit SchedulerSequence(Scheduler* scheduler);

  // Note: this drops tasks not executed yet.
  ~SchedulerSequence() override;

  // SingleTaskSequence implementation.
  SequenceId GetSequenceId() override;

  bool ShouldYield() override;

  void ScheduleTask(base::OnceClosure task,
                    std::vector<SyncToken> sync_token_fences) override;

  void ContinueTask(base::OnceClosure task) override;

 private:
  Scheduler* const scheduler_;
  const SequenceId sequence_id_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerSequence);
};

}  // namespace gpu

#endif  // GPU_IPC_SCHEDULER_SEQUENCE_H_
