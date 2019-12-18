// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/performance_manager_registry_impl.h"

#include "components/performance_manager/performance_manager_test_harness.h"
#include "components/performance_manager/public/performance_manager_main_thread_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class PerformanceManagerRegistryImplTest
    : public PerformanceManagerTestHarness {
 public:
  PerformanceManagerRegistryImplTest() = default;
};

class LenientMockObserver : public PerformanceManagerMainThreadObserver {
 public:
  LenientMockObserver() = default;
  ~LenientMockObserver() override = default;

  MOCK_METHOD1(OnPageNodeCreatedForWebContents, void(content::WebContents*));
};

using MockObserver = ::testing::StrictMock<LenientMockObserver>;

}  // namespace

TEST_F(PerformanceManagerRegistryImplTest,
       ObserverOnPageNodeCreatedForWebContents) {
  MockObserver observer;
  PerformanceManagerRegistryImpl* registry =
      PerformanceManagerRegistryImpl::GetInstance();
  registry->AddObserver(&observer);

  std::unique_ptr<content::WebContents> contents =
      content::RenderViewHostTestHarness::CreateTestWebContents();
  EXPECT_CALL(observer, OnPageNodeCreatedForWebContents(contents.get()));
  registry->CreatePageNodeForWebContents(contents.get());
  testing::Mock::VerifyAndClear(&observer);

  registry->RemoveObserver(&observer);
}

}  // namespace performance_manager
