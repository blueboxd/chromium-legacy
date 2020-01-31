// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_tab_helper.h"

#include "base/test/task_environment.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for TabIdTabHelper class.
class BreadcrumbManagerTabHelperTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();

    first_web_state_.SetBrowserState(chrome_browser_state_.get());
    second_web_state_.SetBrowserState(chrome_browser_state_.get());

    breadcrumb_service_ = static_cast<BreadcrumbManagerKeyedService*>(
        BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
            chrome_browser_state_.get()));
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  web::TestWebState first_web_state_;
  web::TestWebState second_web_state_;
  BreadcrumbManagerKeyedService* breadcrumb_service_;
};

// Tests that the identifier returned for a WebState is unique.
TEST_F(BreadcrumbManagerTabHelperTest, UniqueIdentifiers) {
  BreadcrumbManagerTabHelper::CreateForWebState(&first_web_state_);
  BreadcrumbManagerTabHelper::CreateForWebState(&second_web_state_);

  int first_tab_identifier =
      BreadcrumbManagerTabHelper::FromWebState(&first_web_state_)
          ->GetUniqueId();
  int second_tab_identifier =
      BreadcrumbManagerTabHelper::FromWebState(&second_web_state_)
          ->GetUniqueId();

  EXPECT_GT(first_tab_identifier, 0);
  EXPECT_GT(second_tab_identifier, 0);
  EXPECT_NE(first_tab_identifier, second_tab_identifier);
}

// Tests that BreadcrumbManagerTabHelper events are logged to the associated
// BreadcrumbManagerKeyedService. This test does not attempt to validate that
// every observer method is correctly called as that is done in the
// WebStateObserverTest tests.
TEST_F(BreadcrumbManagerTabHelperTest, EventsLogged) {
  BreadcrumbManagerTabHelper::CreateForWebState(&first_web_state_);

  EXPECT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());
  web::FakeNavigationContext context;
  first_web_state_.OnNavigationStarted(&context);
  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());
  EXPECT_NE(std::string::npos, events.back().find("DidStartNavigation"));
  first_web_state_.OnNavigationFinished(&context);
  events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(2ul, events.size());
  EXPECT_NE(std::string::npos, events.back().find("DidFinishNavigation"));
}

// Tests that BreadcrumbManagerTabHelper events logged from seperate WebStates
// are unique.
TEST_F(BreadcrumbManagerTabHelperTest, UniqueEvents) {
  BreadcrumbManagerTabHelper::CreateForWebState(&first_web_state_);
  web::FakeNavigationContext context;
  first_web_state_.OnNavigationStarted(&context);

  BreadcrumbManagerTabHelper::CreateForWebState(&second_web_state_);
  second_web_state_.OnNavigationStarted(&context);

  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(2ul, events.size());
  EXPECT_STRNE(events.front().c_str(), events.back().c_str());
  EXPECT_NE(std::string::npos, events.front().find("DidStartNavigation"));
  EXPECT_NE(std::string::npos, events.back().find("DidStartNavigation"));
}
