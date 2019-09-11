// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/test_support/graph_test_harness.h"

#include "base/bind.h"
#include "base/run_loop.h"

namespace performance_manager {

GraphTestHarness::GraphTestHarness()
    : task_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME,
                base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}

GraphTestHarness::~GraphTestHarness() {
  // Ideally this would be done in TearDown(), but that would require subclasses
  // do destroy all their nodes before invoking TearDown below.
  graph_.TearDown();
}

void GraphTestHarness::TearDown() {
  base::RunLoop().RunUntilIdle();
}

}  // namespace performance_manager
