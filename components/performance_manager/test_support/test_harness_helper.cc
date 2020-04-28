// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/test_harness_helper.h"

#include "base/run_loop.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/performance_manager/performance_manager_impl.h"

namespace performance_manager {

PerformanceManagerTestHarnessHelper::PerformanceManagerTestHarnessHelper() =
    default;
PerformanceManagerTestHarnessHelper::~PerformanceManagerTestHarnessHelper() =
    default;

void PerformanceManagerTestHarnessHelper::SetUp() {
  // Allow this to be called multiple times.
  if (perf_man_.get())
    return;
  perf_man_ = PerformanceManagerImpl::Create(base::DoNothing());
  registry_ = PerformanceManagerRegistry::Create();
}

void PerformanceManagerTestHarnessHelper::TearDown() {
  // Have the performance manager destroy itself.
  registry_->TearDown();
  registry_.reset();

  base::RunLoop run_loop;
  PerformanceManagerImpl::SetOnDestroyedCallbackForTesting(
      run_loop.QuitClosure());
  PerformanceManagerImpl::Destroy(std::move(perf_man_));
  run_loop.Run();
}

void PerformanceManagerTestHarnessHelper::OnWebContentsCreated(
    content::WebContents* contents) {
  registry_->CreatePageNodeForWebContents(contents);
}

}  // namespace performance_manager
