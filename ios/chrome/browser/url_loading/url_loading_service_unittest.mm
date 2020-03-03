// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/url_loading_service.h"

#import <Foundation/Foundation.h>
#import <PassKit/PassKit.h>

#include <memory>

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/tabs/tab_helper_util.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/url_loading/app_url_loading_service.h"
#import "ios/chrome/browser/url_loading/test_app_url_loading_service.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/url_loading/url_loading_service_factory.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/tab_insertion_browser_agent.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_usage_enabler_browser_agent.h"
#include "ios/chrome/test/block_cleanup_test.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface URLLoadingServiceTestDelegate : NSObject <URLLoadingServiceDelegate>
@end

@implementation URLLoadingServiceTestDelegate

#pragma mark - URLLoadingServiceDelegate

- (void)openURLInNewTabWithCommand:(OpenNewTabCommand*)command {
}

- (void)animateOpenBackgroundTabFromParams:(const UrlLoadParams&)params
                                completion:(void (^)())completion {
}

@end

#pragma mark -

namespace {
class URLLoadingServiceTest : public BlockCleanupTest {
 public:
  URLLoadingServiceTest()
      : browser_(std::make_unique<TestBrowser>()),
        chrome_browser_state_(browser_->GetBrowserState()),
        otr_browser_state_(
            chrome_browser_state_->GetOffTheRecordChromeBrowserState()),
        url_loading_delegate_([[URLLoadingServiceTestDelegate alloc] init]),
        app_service_(std::make_unique<TestAppUrlLoadingService>()),
        service_(UrlLoadingServiceFactory::GetForBrowserState(
            chrome_browser_state_)),
        otr_browser_(std::make_unique<TestBrowser>(otr_browser_state_)),
        otr_service_(
            UrlLoadingServiceFactory::GetForBrowserState(otr_browser_state_)) {
    // Configure app service.
    app_service_->currentBrowserState = chrome_browser_state_;

    // Disable web usage on both browsers
    WebUsageEnablerBrowserAgent::CreateForBrowser(browser_.get());
    WebUsageEnablerBrowserAgent* enabler =
        WebUsageEnablerBrowserAgent::FromBrowser(browser_.get());
    enabler->SetWebUsageEnabled(false);
    WebUsageEnablerBrowserAgent::CreateForBrowser(otr_browser_.get());
    WebUsageEnablerBrowserAgent* otr_enabler =
        WebUsageEnablerBrowserAgent::FromBrowser(otr_browser_.get());
    otr_enabler->SetWebUsageEnabled(false);

    // Create insertion agents and configure services.
    TabInsertionBrowserAgent::CreateForBrowser(browser_.get());
    service_->SetDelegate(url_loading_delegate_);
    service_->SetBrowser(browser_.get());
    service_->SetAppService(app_service_.get());

    TabInsertionBrowserAgent::CreateForBrowser(otr_browser_.get());
    otr_service_->SetDelegate(url_loading_delegate_);
    otr_service_->SetBrowser(otr_browser_.get());
    otr_service_->SetAppService(app_service_.get());
  }

  void TearDown() override {
    // Cleanup to avoid debugger crash in non empty observer lists.
    WebStateList* web_state_list = browser_->GetWebStateList();
    web_state_list->CloseAllWebStates(
        WebStateList::ClosingFlags::CLOSE_NO_FLAGS);
    WebStateList* otr_web_state_list = otr_browser_->GetWebStateList();
    otr_web_state_list->CloseAllWebStates(
        WebStateList::ClosingFlags::CLOSE_NO_FLAGS);

    BlockCleanupTest::TearDown();
  }

  // Returns a new unique_ptr containing a test webstate.
  std::unique_ptr<web::TestWebState> CreateTestWebState() {
    auto web_state = std::make_unique<web::TestWebState>();
    web_state->SetBrowserState(chrome_browser_state_);
    web_state->SetNavigationManager(
        std::make_unique<web::TestNavigationManager>());
    return web_state;
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<Browser> browser_;
  ChromeBrowserState* chrome_browser_state_;
  ChromeBrowserState* otr_browser_state_;
  std::unique_ptr<web::WebState> webState_;
  URLLoadingServiceTestDelegate* url_loading_delegate_;
  std::unique_ptr<TestAppUrlLoadingService> app_service_;
  UrlLoadingService* service_;
  std::unique_ptr<Browser> otr_browser_;
  UrlLoadingService* otr_service_;
};

TEST_F(URLLoadingServiceTest, TestSwitchToTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());

  std::unique_ptr<web::TestWebState> web_state = CreateTestWebState();
  web::WebState* web_state_ptr = web_state.get();
  web_state->SetCurrentURL(GURL("http://test/1"));
  web_state_list->InsertWebState(0, std::move(web_state),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());

  std::unique_ptr<web::TestWebState> web_state_2 = CreateTestWebState();
  web::WebState* web_state_ptr_2 = web_state_2.get();
  GURL url("http://test/2");
  web_state_2->SetCurrentURL(url);
  web_state_list->InsertWebState(1, std::move(web_state_2),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());

  web_state_list->ActivateWebStateAt(0);

  ASSERT_EQ(web_state_ptr, web_state_list->GetActiveWebState());

  service_->Load(
      UrlLoadParams::SwitchToTab(web::NavigationManager::WebLoadParams(url)));
  EXPECT_EQ(web_state_ptr_2, web_state_list->GetActiveWebState());
  EXPECT_EQ(0, app_service_->load_new_tab_call_count);
}

// Tests that switch to open tab from the NTP close it if it doesn't have
// navigation history.
TEST_F(URLLoadingServiceTest, TestSwitchToTabFromNTP) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());

  std::unique_ptr<web::TestWebState> web_state = CreateTestWebState();
  web::WebState* web_state_ptr = web_state.get();
  web_state->SetCurrentURL(GURL("chrome://newtab"));
  web_state_list->InsertWebState(0, std::move(web_state),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());

  std::unique_ptr<web::TestWebState> web_state_2 = CreateTestWebState();
  web::WebState* web_state_ptr_2 = web_state_2.get();
  GURL url("http://test/2");
  web_state_2->SetCurrentURL(url);
  web_state_list->InsertWebState(1, std::move(web_state_2),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());

  web_state_list->ActivateWebStateAt(0);

  ASSERT_EQ(web_state_ptr, web_state_list->GetActiveWebState());

  service_->Load(
      UrlLoadParams::SwitchToTab(web::NavigationManager::WebLoadParams(url)));
  EXPECT_EQ(web_state_ptr_2, web_state_list->GetActiveWebState());
  EXPECT_EQ(1, web_state_list->count());
  EXPECT_EQ(0, app_service_->load_new_tab_call_count);
}

// Tests that trying to switch to a closed tab open from the NTP opens it in the
// NTP.
TEST_F(URLLoadingServiceTest, TestSwitchToClosedTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());

  std::unique_ptr<web::TestWebState> web_state = CreateTestWebState();
  web_state->SetCurrentURL(GURL("chrome://newtab"));
  web::WebState* web_state_ptr = web_state.get();
  web_state_list->InsertWebState(0, std::move(web_state),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());
  web_state_list->ActivateWebStateAt(0);

  GURL url("http://test/2");

  service_->Load(
      UrlLoadParams::SwitchToTab(web::NavigationManager::WebLoadParams(url)));
  EXPECT_EQ(1, web_state_list->count());
  EXPECT_EQ(web_state_ptr, web_state_list->GetActiveWebState());
  EXPECT_EQ(0, app_service_->load_new_tab_call_count);
}

// Tests open a new url in the NTP or the current tab.
TEST_F(URLLoadingServiceTest, TestOpenInCurrentTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());

  // Set a new tab, so we can open in it.
  GURL newtab("chrome://newtab");
  service_->Load(
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(newtab)));
  EXPECT_EQ(1, web_state_list->count());

  // Test opening this url over NTP.
  GURL url1("http://test/1");
  service_->Load(
      UrlLoadParams::InCurrentTab(web::NavigationManager::WebLoadParams(url1)));

  // We won't to wait for the navigation item to be committed, let's just
  // make sure it is at least pending.
  EXPECT_EQ(url1, web_state_list->GetActiveWebState()
                      ->GetNavigationManager()
                      ->GetPendingItem()
                      ->GetOriginalRequestURL());
  // And that a new tab wasn't created.
  EXPECT_EQ(1, web_state_list->count());

  // Check that we had no app level redirection.
  EXPECT_EQ(0, app_service_->load_new_tab_call_count);
}

// Tests opening a url in a new tab.
TEST_F(URLLoadingServiceTest, TestOpenInNewTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());

  // Set a new tab.
  GURL newtab("chrome://newtab");
  service_->Load(
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(newtab)));
  EXPECT_EQ(1, web_state_list->count());

  // Open another one.
  GURL url("http://test/2");
  service_->Load(
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url)));
  EXPECT_EQ(2, web_state_list->count());

  // Check that we had no app level redirection.
  EXPECT_EQ(0, app_service_->load_new_tab_call_count);
}

// Tests open a new url in the current incognito tab.
TEST_F(URLLoadingServiceTest, TestOpenInCurrentIncognitoTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());
  WebStateList* otr_web_state_list = otr_browser_->GetWebStateList();
  ASSERT_EQ(0, otr_web_state_list->count());

  // Make app level to be otr.
  ChromeBrowserState* otr_browser_state =
      chrome_browser_state_->GetOffTheRecordChromeBrowserState();
  app_service_->currentBrowserState = otr_browser_state;

  // Set a new tab.
  GURL newtab("chrome://newtab");
  UrlLoadParams new_tab_params =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(newtab));
  new_tab_params.in_incognito = YES;
  otr_service_->Load(new_tab_params);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(1, otr_web_state_list->count());

  // Open otr request with otr service.
  GURL url1("http://test/1");
  UrlLoadParams params1 =
      UrlLoadParams::InCurrentTab(web::NavigationManager::WebLoadParams(url1));
  params1.in_incognito = YES;
  otr_service_->Load(params1);

  // We won't to wait for the navigation item to be committed, let's just
  // make sure it is at least pending.
  EXPECT_EQ(url1, otr_web_state_list->GetActiveWebState()
                      ->GetNavigationManager()
                      ->GetPendingItem()
                      ->GetOriginalRequestURL());

  // And that a new tab wasn't created.
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(1, otr_web_state_list->count());

  // Check that we had no app level redirection.
  EXPECT_EQ(0, app_service_->load_new_tab_call_count);
}

// Tests opening a url in a new incognito tab.
TEST_F(URLLoadingServiceTest, TestOpenInNewIncognitoTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());
  WebStateList* otr_web_state_list = otr_browser_->GetWebStateList();
  ASSERT_EQ(0, otr_web_state_list->count());

  ChromeBrowserState* otr_browser_state =
      chrome_browser_state_->GetOffTheRecordChromeBrowserState();
  app_service_->currentBrowserState = otr_browser_state;

  GURL url1("http://test/1");
  UrlLoadParams params1 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url1));
  params1.in_incognito = YES;
  otr_service_->Load(params1);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(1, otr_web_state_list->count());

  GURL url2("http://test/2");
  UrlLoadParams params2 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url2));
  params2.in_incognito = YES;
  otr_service_->Load(params2);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(2, otr_web_state_list->count());

  // Check if we had any app level redirection.
  EXPECT_EQ(0, app_service_->load_new_tab_call_count);
}

// Test opening a normal url in new tab with incognito service.
TEST_F(URLLoadingServiceTest, TestOpenNormalInNewTabWithIncognitoService) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());
  WebStateList* otr_web_state_list = otr_browser_->GetWebStateList();
  ASSERT_EQ(0, otr_web_state_list->count());

  ChromeBrowserState* otr_browser_state =
      chrome_browser_state_->GetOffTheRecordChromeBrowserState();
  app_service_->currentBrowserState = otr_browser_state;

  // Send to right service.
  GURL url1("http://test/1");
  UrlLoadParams params1 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url1));
  params1.in_incognito = YES;
  otr_service_->Load(params1);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(1, otr_web_state_list->count());

  // Send to wrong service.
  GURL url2("http://test/2");
  UrlLoadParams params2 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url2));
  params2.in_incognito = NO;
  otr_service_->Load(params2);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(1, otr_web_state_list->count());

  // Check that had one app level redirection.
  EXPECT_EQ(1, app_service_->load_new_tab_call_count);
}

// Test opening an incognito url in new tab with normal service.
TEST_F(URLLoadingServiceTest, TestOpenIncognitoInNewTabWithNormalService) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());
  WebStateList* otr_web_state_list = otr_browser_->GetWebStateList();
  ASSERT_EQ(0, otr_web_state_list->count());

  app_service_->currentBrowserState = chrome_browser_state_;

  // Send to wrong service.
  GURL url1("http://test/1");
  UrlLoadParams params1 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url1));
  params1.in_incognito = YES;
  service_->Load(params1);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(0, otr_web_state_list->count());

  // Send to right service.
  GURL url2("http://test/2");
  UrlLoadParams params2 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url2));
  params2.in_incognito = NO;
  service_->Load(params2);
  EXPECT_EQ(1, web_state_list->count());
  EXPECT_EQ(0, otr_web_state_list->count());

  // Check that we had one app level redirection.
  EXPECT_EQ(1, app_service_->load_new_tab_call_count);
}

// Test opening an incognito url in new tab with normal service using load
// strategy.
TEST_F(URLLoadingServiceTest, TestOpenIncognitoInNewTabWithLoadStrategy) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());
  WebStateList* otr_web_state_list = otr_browser_->GetWebStateList();
  ASSERT_EQ(0, otr_web_state_list->count());

  app_service_->currentBrowserState = chrome_browser_state_;

  // Send to normal service.
  GURL url1("http://test/1");
  UrlLoadParams params1 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url1));
  params1.load_strategy = UrlLoadStrategy::ALWAYS_IN_INCOGNITO;
  service_->Load(params1);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(0, otr_web_state_list->count());

  // Check that we had one app level redirection.
  EXPECT_EQ(1, app_service_->load_new_tab_call_count);
}

// Test opening an incognito url in current tab with normal service using load
// strategy.
TEST_F(URLLoadingServiceTest, TestOpenIncognitoInCurrentTabWithLoadStrategy) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());
  WebStateList* otr_web_state_list = otr_browser_->GetWebStateList();
  ASSERT_EQ(0, otr_web_state_list->count());

  // Make app level to be otr.
  ChromeBrowserState* otr_browser_state =
      chrome_browser_state_->GetOffTheRecordChromeBrowserState();
  app_service_->currentBrowserState = otr_browser_state;

  // Set a new incognito tab.
  GURL newtab("chrome://newtab");
  UrlLoadParams new_tab_params =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(newtab));
  new_tab_params.in_incognito = YES;
  otr_service_->Load(new_tab_params);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(1, otr_web_state_list->count());

  // Send to normal service.
  GURL url1("http://test/1");
  UrlLoadParams params1 =
      UrlLoadParams::InCurrentTab(web::NavigationManager::WebLoadParams(url1));
  params1.load_strategy = UrlLoadStrategy::ALWAYS_IN_INCOGNITO;
  service_->Load(params1);

  // We won't to wait for the navigation item to be committed, let's just
  // make sure it is at least pending.
  EXPECT_EQ(url1, otr_web_state_list->GetActiveWebState()
                      ->GetNavigationManager()
                      ->GetPendingItem()
                      ->GetOriginalRequestURL());

  // And that a new tab wasn't created.
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(1, otr_web_state_list->count());

  // Check that we had no app level redirection.
  EXPECT_EQ(0, app_service_->load_new_tab_call_count);
}

}  // namespace
