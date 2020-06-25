// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gl_surface_task_scheduler.h"

#include "base/synchronization/lock.h"

namespace gpu {

GLSurfaceTaskScheduler::GLSurfaceTaskScheduler(Scheduler* scheduler)
    : scheduler_(scheduler),
      sequence_id_(scheduler_->CreateSequence(SchedulingPriority::kHigh)) {}

GLSurfaceTaskScheduler::~GLSurfaceTaskScheduler() {
  scheduler_->DestroySequence(sequence_id_);
}

void GLSurfaceTaskScheduler::ScheduleTask(base::OnceClosure closure) {
  Scheduler::Task task(sequence_id_, std::move(closure), {});
  scheduler_->ScheduleTask(std::move(task));
}

}  // namespace gpu
