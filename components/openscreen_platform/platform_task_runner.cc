// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrono>  // NOLINT
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"

#include "components/openscreen_platform/platform_task_runner.h"

namespace openscreen_platform {

using openscreen::Clock;
using openscreen::TaskRunner;

namespace {
void ExecuteTask(TaskRunner::Task task) {
  task();
}
}  // namespace

PlatformTaskRunner::PlatformTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  task_runner_ = task_runner;
}

PlatformTaskRunner::~PlatformTaskRunner() = default;

void PlatformTaskRunner::PostPackagedTask(TaskRunner::Task task) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(ExecuteTask, std::move(task)));
}

void PlatformTaskRunner::PostPackagedTaskWithDelay(TaskRunner::Task task,
                                                   Clock::duration delay) {
  auto time_delta = base::TimeDelta::FromMicroseconds(
      std::chrono::duration_cast<std::chrono::microseconds>(delay).count());
  task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(ExecuteTask, std::move(task)), time_delta);
}

}  // namespace openscreen_platform
