// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_tab_helper.h"

#include "base/strings/string_split.h"
#include "base/test/task_environment.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#import "ios/web/public/test/error_test_util.h"
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
  EXPECT_NE(std::string::npos,
            events.back().find(kBreadcrumbDidStartNavigation))
      << events.back();

  first_web_state_.OnNavigationFinished(&context);
  events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(2ul, events.size());
  EXPECT_NE(std::string::npos,
            events.back().find(kBreadcrumbDidFinishNavigation))
      << events.back();
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
  EXPECT_NE(std::string::npos,
            events.front().find(kBreadcrumbDidStartNavigation))
      << events.front();
  EXPECT_NE(std::string::npos,
            events.back().find(kBreadcrumbDidStartNavigation))
      << events.back();
}

// Tests metadata for chrome://newtab NTP navigation.
TEST_F(BreadcrumbManagerTabHelperTest, ChromeNewTabNavigationStart) {
  BreadcrumbManagerTabHelper::CreateForWebState(&first_web_state_);
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kChromeUINewTabURL));
  first_web_state_.OnNavigationStarted(&context);
  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos, events.front().find(base::StringPrintf(
                                   "%s%lld", kBreadcrumbDidStartNavigation,
                                   context.GetNavigationId())))
      << events.front();
  EXPECT_NE(std::string::npos, events.front().find(kBreadcrumbNtpNavigation))
      << events.front();
}

// Tests metadata for about://newtab NTP navigation.
TEST_F(BreadcrumbManagerTabHelperTest, AboutNewTabNavigationStart) {
  BreadcrumbManagerTabHelper::CreateForWebState(&first_web_state_);
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  web::FakeNavigationContext context;
  context.SetUrl(GURL("about://newtab"));
  first_web_state_.OnNavigationStarted(&context);
  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos, events.front().find(base::StringPrintf(
                                   "%s%lld", kBreadcrumbDidStartNavigation,
                                   context.GetNavigationId())))
      << events.front();
  EXPECT_NE(std::string::npos, events.front().find(kBreadcrumbNtpNavigation))
      << events.front();
}

// Tests unique ID in DidStartNavigation and DidStartNavigation.
TEST_F(BreadcrumbManagerTabHelperTest, NavigationUniqueId) {
  BreadcrumbManagerTabHelper::CreateForWebState(&first_web_state_);

  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  // DidStartNavigation
  web::FakeNavigationContext context;
  first_web_state_.OnNavigationStarted(&context);
  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos, events.front().find(base::StringPrintf(
                                   "%s%lld", kBreadcrumbDidStartNavigation,
                                   context.GetNavigationId())))
      << events.front();

  // DidFinishNavigation
  first_web_state_.OnNavigationFinished(&context);
  events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(2ul, events.size());
  EXPECT_NE(std::string::npos, events.back().find(base::StringPrintf(
                                   "%s%lld", kBreadcrumbDidFinishNavigation,
                                   context.GetNavigationId())))
      << events.back();
}

// Tests renderer initiated metadata in DidStartNavigation.
TEST_F(BreadcrumbManagerTabHelperTest, RendererInitiatedByUser) {
  BreadcrumbManagerTabHelper::CreateForWebState(&first_web_state_);
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  web::FakeNavigationContext context;
  context.SetIsRendererInitiated(true);
  context.SetHasUserGesture(true);
  first_web_state_.OnNavigationStarted(&context);
  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos, events.back().find("#link")) << events.back();
  EXPECT_NE(std::string::npos,
            events.back().find(kBreadcrumbDidStartNavigation))
      << events.back();
  EXPECT_NE(std::string::npos,
            events.back().find(kBreadcrumbRendererInitiatedByUser))
      << events.back();
  EXPECT_EQ(std::string::npos,
            events.back().find(kBreadcrumbRendererInitiatedByScript))
      << events.back();
}

// Tests renderer initiated metadata in DidStartNavigation.
TEST_F(BreadcrumbManagerTabHelperTest, RendererInitiatedByScript) {
  BreadcrumbManagerTabHelper::CreateForWebState(&first_web_state_);
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  web::FakeNavigationContext context;
  context.SetIsRendererInitiated(true);
  context.SetHasUserGesture(false);
  context.SetPageTransition(ui::PAGE_TRANSITION_RELOAD);
  first_web_state_.OnNavigationStarted(&context);
  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos, events.back().find("#reload")) << events.back();
  EXPECT_NE(std::string::npos,
            events.back().find(kBreadcrumbDidStartNavigation))
      << events.back();
  EXPECT_EQ(std::string::npos,
            events.back().find(kBreadcrumbRendererInitiatedByUser))
      << events.back();
  EXPECT_NE(std::string::npos,
            events.back().find(kBreadcrumbRendererInitiatedByScript))
      << events.back();
}

// Tests browser initiated metadata in DidStartNavigation.
TEST_F(BreadcrumbManagerTabHelperTest, BrowserInitiatedByScript) {
  BreadcrumbManagerTabHelper::CreateForWebState(&first_web_state_);
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  web::FakeNavigationContext context;
  context.SetIsRendererInitiated(false);
  context.SetPageTransition(ui::PAGE_TRANSITION_TYPED);
  first_web_state_.OnNavigationStarted(&context);
  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos, events.back().find("#typed")) << events.back();
  EXPECT_NE(std::string::npos,
            events.back().find(kBreadcrumbDidStartNavigation))
      << events.back();
  EXPECT_EQ(std::string::npos,
            events.back().find(kBreadcrumbRendererInitiatedByUser))
      << events.back();
  EXPECT_EQ(std::string::npos,
            events.back().find(kBreadcrumbRendererInitiatedByScript))
      << events.back();
}

// Tests download navigation.
TEST_F(BreadcrumbManagerTabHelperTest, Download) {
  BreadcrumbManagerTabHelper::CreateForWebState(&first_web_state_);
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  web::FakeNavigationContext context;
  context.SetIsDownload(true);
  first_web_state_.OnNavigationFinished(&context);
  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos,
            events.back().find(kBreadcrumbDidFinishNavigation))
      << events.back();
  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbDownload))
      << events.back();
}

// Tests page load succeess.
TEST_F(BreadcrumbManagerTabHelperTest, PageLoadSuccess) {
  BreadcrumbManagerTabHelper::CreateForWebState(&first_web_state_);
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  first_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbPageLoaded))
      << events.back();
  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbPageLoadSuccess))
      << events.back();
  EXPECT_EQ(std::string::npos, events.back().find(kBreadcrumbPageLoadFailure))
      << events.back();
}

// Tests page load failure.
TEST_F(BreadcrumbManagerTabHelperTest, PageLoadFailure) {
  BreadcrumbManagerTabHelper::CreateForWebState(&first_web_state_);
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  first_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbPageLoaded))
      << events.back();
  EXPECT_EQ(std::string::npos, events.back().find(kBreadcrumbPageLoadSuccess))
      << events.back();
  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbPageLoadFailure))
      << events.back();
}

// Tests NTP page load.
TEST_F(BreadcrumbManagerTabHelperTest, NtpPageLoad) {
  BreadcrumbManagerTabHelper::CreateForWebState(&first_web_state_);
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  first_web_state_.SetCurrentURL(GURL(kChromeUINewTabURL));
  first_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbPageLoaded))
      << events.back();
  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbNtpNavigation))
      << events.back();
  // NTP navigation can't fail, so there is no success/failure metadata.
  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbPageLoaded))
      << events.back();
  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbNtpNavigation))
      << events.back();
}

// Tests navigation error.
TEST_F(BreadcrumbManagerTabHelperTest, NavigationError) {
  BreadcrumbManagerTabHelper::CreateForWebState(&first_web_state_);
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  web::FakeNavigationContext context;
  NSError* error = web::testing::CreateTestNetError([NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorNotConnectedToInternet
             userInfo:nil]);
  context.SetError(error);
  first_web_state_.OnNavigationFinished(&context);
  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos,
            events.back().find(kBreadcrumbDidFinishNavigation))
      << events.back();
  EXPECT_NE(std::string::npos, events.back().find(net::ErrorToShortString(
                                   net::ERR_INTERNET_DISCONNECTED)))
      << events.back();
}
