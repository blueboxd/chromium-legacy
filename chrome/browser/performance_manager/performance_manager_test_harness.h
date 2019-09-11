// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_TEST_HARNESS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_TEST_HARNESS_H_

#include "chrome/browser/performance_manager/performance_manager_impl.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager {

// A test harness that initializes PerformanceManagerImpl, plus the entire
// RenderViewHost harness. Allows for creating full WebContents, and their
// accompanying structures in the graph. The task environment is accessed
// via content::RenderViewHostTestHarness::test_bundle().
class PerformanceManagerTestHarness : public ChromeRenderViewHostTestHarness {
 public:
  using Super = ChromeRenderViewHostTestHarness;

  PerformanceManagerTestHarness();
  ~PerformanceManagerTestHarness() override;

  void SetUp() override;
  void TearDown() override;

  // Creates a test web contents with performance manager tab helpers
  // attached. This is a test web contents that can be interacted with
  // via WebContentsTester.
  std::unique_ptr<content::WebContents> CreateTestWebContents();

 private:
  std::unique_ptr<PerformanceManagerImpl> perf_man_;

  DISALLOW_COPY_AND_ASSIGN(PerformanceManagerTestHarness);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_TEST_HARNESS_H_
