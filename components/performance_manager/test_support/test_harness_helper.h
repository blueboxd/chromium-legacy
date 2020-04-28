// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common functionality for unittest and browsertest harnesses.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_TEST_HARNESS_HELPER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_TEST_HARNESS_HELPER_H_

#include <memory>

namespace content {
class WebContents;
}  // namespace content

namespace performance_manager {

class PerformanceManagerImpl;
class PerformanceManagerRegistry;

class PerformanceManagerTestHarnessHelper {
 public:
  PerformanceManagerTestHarnessHelper();
  PerformanceManagerTestHarnessHelper(
      const PerformanceManagerTestHarnessHelper&) = delete;
  PerformanceManagerTestHarnessHelper& operator=(
      const PerformanceManagerTestHarnessHelper&) = delete;
  ~PerformanceManagerTestHarnessHelper();

  // Sets up the PM and registry, etc.
  void SetUp();

  // Tears down the PM and registry, etc. Blocks on the main thread until they
  // are torn down.
  void TearDown();

  // Attaches tab helpers to the provided |contents|.
  void OnWebContentsCreated(content::WebContents* contents);

 private:
  std::unique_ptr<PerformanceManagerImpl> perf_man_;
  std::unique_ptr<PerformanceManagerRegistry> registry_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_TEST_HARNESS_HELPER_H_
