// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/performance_manager_test_harness.h"

#include "content/public/browser/web_contents.h"

namespace performance_manager {

PerformanceManagerTestHarness::PerformanceManagerTestHarness() = default;

PerformanceManagerTestHarness::~PerformanceManagerTestHarness() = default;

void PerformanceManagerTestHarness::SetUp() {
  Super::SetUp();
  helper_ = std::make_unique<PerformanceManagerTestHarnessHelper>();
  helper_->SetUp();
}

void PerformanceManagerTestHarness::TearDown() {
  if (helper_)
    TearDownNow();
  Super::TearDown();
}

std::unique_ptr<content::WebContents>
PerformanceManagerTestHarness::CreateTestWebContents() {
  std::unique_ptr<content::WebContents> contents =
      Super::CreateTestWebContents();
  helper_->OnWebContentsCreated(contents.get());
  return contents;
}

void PerformanceManagerTestHarness::TearDownNow() {
  helper_->TearDown();
  helper_.reset();
}

}  // namespace performance_manager
