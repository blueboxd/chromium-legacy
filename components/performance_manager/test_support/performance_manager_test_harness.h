// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PERFORMANCE_MANAGER_TEST_HARNESS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PERFORMANCE_MANAGER_TEST_HARNESS_H_

#include "components/performance_manager/test_support/test_harness_helper.h"
#include "content/public/test/test_renderer_host.h"

namespace performance_manager {

// A test harness that initializes PerformanceManagerImpl, plus the entire
// RenderViewHost harness. Allows for creating full WebContents, and their
// accompanying structures in the graph. The task environment is accessed
// via content::RenderViewHostTestHarness::test_bundle(). RenderFrameHosts and
// such are not created, so this is suitable for unittests but not browsertests.
class PerformanceManagerTestHarness
    : public content::RenderViewHostTestHarness {
 public:
  using Super = content::RenderViewHostTestHarness;

  PerformanceManagerTestHarness();
  PerformanceManagerTestHarness(const PerformanceManagerTestHarness&) = delete;
  PerformanceManagerTestHarness& operator=(
      const PerformanceManagerTestHarness&) = delete;
  ~PerformanceManagerTestHarness() override;

  void SetUp() override;
  void TearDown() override;

  // Creates a test web contents with performance manager tab helpers
  // attached. This is a test web contents that can be interacted with
  // via WebContentsTester.
  std::unique_ptr<content::WebContents> CreateTestWebContents();

 private:
  std::unique_ptr<PerformanceManagerTestHarnessHelper> helper_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PERFORMANCE_MANAGER_TEST_HARNESS_H_
