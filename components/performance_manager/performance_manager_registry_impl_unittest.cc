// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/performance_manager_registry_impl.h"

#include "base/memory/ptr_util.h"
#include "components/performance_manager/public/performance_manager_main_thread_mechanism.h"
#include "components/performance_manager/public/performance_manager_main_thread_observer.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_navigation_throttle.h"
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

class LenientMockMechanism : public PerformanceManagerMainThreadMechanism {
 public:
  LenientMockMechanism() = default;
  ~LenientMockMechanism() override = default;

  void ExpectCallToCreateThrottlesForNavigation(
      content::NavigationHandle* handle,
      Throttles throttles_to_return) {
    throttles_to_return_ = std::move(throttles_to_return);
    EXPECT_CALL(*this, OnCreateThrottlesForNavigation(handle));
  }

 private:
  MOCK_METHOD1(OnCreateThrottlesForNavigation,
               void(content::NavigationHandle*));

  // PerformanceManagerMainThreadMechanism implementation:
  // GMock doesn't support move-only types, so we use a custom wrapper to work
  // around this.
  Throttles CreateThrottlesForNavigation(
      content::NavigationHandle* handle) override {
    OnCreateThrottlesForNavigation(handle);
    return std::move(throttles_to_return_);
  }

  Throttles throttles_to_return_;
};

using MockMechanism = ::testing::StrictMock<LenientMockMechanism>;

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

TEST_F(PerformanceManagerRegistryImplTest,
       MechanismCreateThrottlesForNavigation) {
  MockMechanism mechanism1, mechanism2;
  PerformanceManagerRegistryImpl* registry =
      PerformanceManagerRegistryImpl::GetInstance();
  registry->AddMechanism(&mechanism1);
  registry->AddMechanism(&mechanism2);

  std::unique_ptr<content::WebContents> contents =
      content::RenderViewHostTestHarness::CreateTestWebContents();
  std::unique_ptr<content::NavigationHandle> handle =
      base::WrapUnique(new content::MockNavigationHandle(contents.get()));
  std::unique_ptr<content::NavigationThrottle> throttle1 =
      base::WrapUnique(new content::TestNavigationThrottle(handle.get()));
  std::unique_ptr<content::NavigationThrottle> throttle2 =
      base::WrapUnique(new content::TestNavigationThrottle(handle.get()));
  std::unique_ptr<content::NavigationThrottle> throttle3 =
      base::WrapUnique(new content::TestNavigationThrottle(handle.get()));
  auto* raw_throttle1 = throttle1.get();
  auto* raw_throttle2 = throttle2.get();
  auto* raw_throttle3 = throttle3.get();
  MockMechanism::Throttles throttles1, throttles2;
  throttles1.push_back(std::move(throttle1));
  throttles2.push_back(std::move(throttle2));
  throttles2.push_back(std::move(throttle3));

  mechanism1.ExpectCallToCreateThrottlesForNavigation(handle.get(),
                                                      std::move(throttles1));
  mechanism2.ExpectCallToCreateThrottlesForNavigation(handle.get(),
                                                      std::move(throttles2));
  auto throttles = registry->CreateThrottlesForNavigation(handle.get());
  testing::Mock::VerifyAndClear(&mechanism1);
  testing::Mock::VerifyAndClear(&mechanism2);

  // Expect that the throttles from both mechanisms were combined into one
  // list.
  ASSERT_EQ(3u, throttles.size());
  EXPECT_EQ(raw_throttle1, throttles[0].get());
  EXPECT_EQ(raw_throttle2, throttles[1].get());
  EXPECT_EQ(raw_throttle3, throttles[2].get());

  registry->RemoveMechanism(&mechanism1);
  registry->RemoveMechanism(&mechanism2);
}

}  // namespace performance_manager
