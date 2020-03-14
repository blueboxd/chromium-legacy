// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_browser_agent.h"

#include "base/bind.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_tab_helper.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for testing BreadcrumbManagerBrowserAgent class.
class BreadcrumbManagerBrowserAgentTest : public PlatformTest {
 protected:
  BreadcrumbManagerBrowserAgentTest() {
    TestChromeBrowserState::Builder test_cbs_builder;
    browser_state_ = test_cbs_builder.Build();

    breadcrumb_service_ = static_cast<BreadcrumbManagerKeyedService*>(
        BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
            browser_state_.get()));
  }

  web::WebTaskEnvironment task_env_{
      web::WebTaskEnvironment::Options::DEFAULT,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  FakeWebStateListDelegate web_state_list_delegate_;
  BreadcrumbManagerKeyedService* breadcrumb_service_;
};

// Tests that an event logged by the BrowserAgent is returned with events for
// the associated |browser_state_|.
TEST_F(BreadcrumbManagerBrowserAgentTest, LogEvent) {
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  // Create and setup Browser.
  WebStateList web_state_list(&web_state_list_delegate_);
  std::unique_ptr<Browser> browser =
      std::make_unique<TestBrowser>(browser_state_.get(), &web_state_list);
  BreadcrumbManagerBrowserAgent::CreateForBrowser(browser.get());

  // Insert WebState into |browser|.
  web::WebState::CreateParams createParams(browser_state_.get());
  std::unique_ptr<web::WebState> web_state =
      web::WebState::Create(createParams);
  InfoBarManagerImpl::CreateForWebState(web_state.get());
  BreadcrumbManagerTabHelper::CreateForWebState(web_state.get());
  browser->GetWebStateList()->InsertWebState(
      /*index=*/0, std::move(web_state),
      WebStateList::InsertionFlags::INSERT_NO_FLAGS, WebStateOpener());

  EXPECT_EQ(1ul, breadcrumb_service_->GetEvents(0).size());
}

// Tests that events logged through BrowserAgents associated with different
// Browser instances are returned with events for the associated
// |browser_state_| and are uniquely identifiable.
TEST_F(BreadcrumbManagerBrowserAgentTest, MultipleBrowsers) {
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  // Create and setup Browser.
  WebStateList web_state_list(&web_state_list_delegate_);
  std::unique_ptr<Browser> browser =
      std::make_unique<TestBrowser>(browser_state_.get(), &web_state_list);
  BreadcrumbManagerBrowserAgent::CreateForBrowser(browser.get());

  // Insert WebState into |browser|.
  web::WebState::CreateParams createParams(browser_state_.get());
  std::unique_ptr<web::WebState> web_state =
      web::WebState::Create(createParams);
  InfoBarManagerImpl::CreateForWebState(web_state.get());
  BreadcrumbManagerTabHelper::CreateForWebState(web_state.get());
  browser->GetWebStateList()->InsertWebState(
      /*index=*/0, std::move(web_state),
      WebStateList::InsertionFlags::INSERT_NO_FLAGS, WebStateOpener());

  // Create and setup second Browser.
  WebStateList web_state_list2(&web_state_list_delegate_);
  std::unique_ptr<Browser> browser2 =
      std::make_unique<TestBrowser>(browser_state_.get(), &web_state_list2);
  BreadcrumbManagerBrowserAgent::CreateForBrowser(browser2.get());

  // Insert WebState into |browser2|.
  std::unique_ptr<web::WebState> web_state2 =
      web::WebState::Create(createParams);
  InfoBarManagerImpl::CreateForWebState(web_state2.get());
  BreadcrumbManagerTabHelper::CreateForWebState(web_state2.get());
  browser2->GetWebStateList()->InsertWebState(
      /*index=*/0, std::move(web_state2),
      WebStateList::InsertionFlags::INSERT_NO_FLAGS, WebStateOpener());

  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  EXPECT_EQ(2ul, events.size());

  // Seperately compare the start and end of the event strings to ensure
  // uniqueness at both the Browser and WebState layer.
  std::size_t browser1_split_pos = events.front().find("Insert");
  std::size_t browser2_split_pos = events.back().find("Insert");
  // The start of the string must be unique to differentiate the associated
  // Browser object by including the BreadcrumbManagerBrowserAgent's
  // |unique_id_|.
  // (The Timestamp will match due to TimeSource::MOCK_TIME in the |task_env_|.)
  std::string browser1_start = events.front().substr(browser1_split_pos);
  std::string browser2_start = events.back().substr(browser2_split_pos);
  EXPECT_STRNE(browser1_start.c_str(), browser2_start.c_str());
  // The end of the string must be unique because the WebStates are different
  // and that needs to be represented in the event string.
  std::string browser1_end = events.front().substr(
      browser1_split_pos, events.front().length() - browser1_split_pos);
  std::string browser2_end = events.back().substr(
      browser2_split_pos, events.back().length() - browser2_split_pos);
  EXPECT_STRNE(browser1_end.c_str(), browser2_end.c_str());
}

// Tests WebStateList's batch insertion and closing.
TEST_F(BreadcrumbManagerBrowserAgentTest, BatchOperations) {
  WebStateList web_state_list(&web_state_list_delegate_);
  std::unique_ptr<Browser> browser =
      std::make_unique<TestBrowser>(browser_state_.get(), &web_state_list);
  BreadcrumbManagerBrowserAgent::CreateForBrowser(browser.get());

  // Insert multiple WebStates.
  web_state_list.PerformBatchOperation(base::BindOnce(^(WebStateList* list) {
    web::WebState::CreateParams create_params(browser_state_.get());
    list->InsertWebState(
        /*index=*/0, web::WebState::Create(create_params),
        WebStateList::InsertionFlags::INSERT_NO_FLAGS, WebStateOpener());
    list->InsertWebState(
        /*index=*/1, web::WebState::Create(create_params),
        WebStateList::InsertionFlags::INSERT_NO_FLAGS, WebStateOpener());
  }));

  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());
  EXPECT_NE(std::string::npos, events.front().find("Inserted 2 tabs"))
      << events.front();

  // Close multiple WebStates.
  web_state_list.PerformBatchOperation(base::BindOnce(^(WebStateList* list) {
    list->CloseWebStateAt(
        /*index=*/0, WebStateList::ClosingFlags::CLOSE_NO_FLAGS);
    list->CloseWebStateAt(
        /*index=*/0, WebStateList::ClosingFlags::CLOSE_NO_FLAGS);
  }));

  events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(2ul, events.size());
  EXPECT_NE(std::string::npos, events.back().find("Closed 2 tabs"))
      << events.back();
}
