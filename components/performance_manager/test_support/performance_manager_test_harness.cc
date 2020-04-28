// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/performance_manager_test_harness.h"

#include "content/public/browser/web_contents.h"

namespace performance_manager {

PerformanceManagerTestHarness::PerformanceManagerTestHarness() {
  helper_ = std::make_unique<PerformanceManagerTestHarnessHelper>();
}

PerformanceManagerTestHarness::~PerformanceManagerTestHarness() = default;

void PerformanceManagerTestHarness::SetUp() {
  Super::SetUp();
  helper_->SetUp();
}

void PerformanceManagerTestHarness::TearDown() {
  helper_->TearDown();
  Super::TearDown();
}

std::unique_ptr<content::WebContents>
PerformanceManagerTestHarness::CreateTestWebContents() {
  std::unique_ptr<content::WebContents> contents =
      Super::CreateTestWebContents();
  helper_->OnWebContentsCreated(contents.get());
  return contents;
}

}  // namespace performance_manager
