// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GL_SURFACE_TASK_SCHEDULER_H_
#define GPU_COMMAND_BUFFER_SERVICE_GL_SURFACE_TASK_SCHEDULER_H_

#include "base/synchronization/lock.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/gpu_export.h"
#include "ui/gl/gl_surface.h"

namespace gpu {

class GPU_EXPORT GLSurfaceTaskScheduler : public gl::GLSurface::TaskScheduler {
 public:
  explicit GLSurfaceTaskScheduler(Scheduler* scheduler);

  // gl::GLSurface::TaskScheduler implementation.
  void ScheduleTask(base::OnceClosure closure) override;

 private:
  ~GLSurfaceTaskScheduler() override;

  Scheduler* const scheduler_;
  const gpu::SequenceId sequence_id_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GL_SURFACE_TASK_SCHEDULER_H_
