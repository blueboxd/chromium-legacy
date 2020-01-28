// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPENSCREEN_PLATFORM_PLATFORM_TASK_RUNNER_H_
#define COMPONENTS_OPENSCREEN_PLATFORM_PLATFORM_TASK_RUNNER_H_

#include "base/single_thread_task_runner.h"
#include "third_party/openscreen/src/platform/api/task_runner.h"
#include "third_party/openscreen/src/platform/api/time.h"

namespace openscreen_platform {

class PlatformTaskRunner final : public openscreen::TaskRunner {
 public:
  explicit PlatformTaskRunner(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  PlatformTaskRunner(const PlatformTaskRunner&) = delete;
  PlatformTaskRunner(PlatformTaskRunner&&) = delete;
  PlatformTaskRunner& operator=(const PlatformTaskRunner&) = delete;
  PlatformTaskRunner& operator=(PlatformTaskRunner&&) = delete;

  // TaskRunner overrides
  ~PlatformTaskRunner() final;
  void PostPackagedTask(openscreen::TaskRunner::Task task) final;
  void PostPackagedTaskWithDelay(openscreen::TaskRunner::Task task,
                                 openscreen::Clock::duration delay) final;

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace openscreen_platform

#endif  // COMPONENTS_OPENSCREEN_PLATFORM_PLATFORM_TASK_RUNNER_H_
