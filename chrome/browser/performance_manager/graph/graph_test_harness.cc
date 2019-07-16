// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/graph_test_harness.h"

#include "base/bind.h"
#include "base/run_loop.h"

namespace performance_manager {

GraphTestHarness::GraphTestHarness()
    : task_env_(
          base::test::ScopedTaskEnvironment::TimeSource::MOCK_TIME,
          base::test::ScopedTaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}

GraphTestHarness::~GraphTestHarness() = default;

void GraphTestHarness::TearDown() {
  base::RunLoop().RunUntilIdle();
}

}  // namespace performance_manager
