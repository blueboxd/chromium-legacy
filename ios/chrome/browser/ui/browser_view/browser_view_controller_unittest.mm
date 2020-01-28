// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller+private.h"

#import <Foundation/Foundation.h>
#import <PassKit/PassKit.h>

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/favicon/favicon_service_factory.h"
#include "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/tabs/tab_helper_util.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_view_controller.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller_dependency_factory.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller_helper.h"
#import "ios/chrome/browser/ui/browser_view/key_commands_provider.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/page_info_commands.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_state_list_web_usage_enabler.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_state_list_web_usage_enabler_factory.h"
#include "ios/chrome/test/block_cleanup_test.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Private methods in BrowserViewController to test.
@interface BrowserViewController (Testing)

- (void)webStateSelected:(web::WebState*)webState
           notifyToolbar:(BOOL)notifyToolbar;
@end

#pragma mark -

namespace {
class BrowserViewControllerTest : public BlockCleanupTest {
 public:
  BrowserViewControllerTest() : web_state_list_(&web_state_list_delegate_) {}

 protected:
  void SetUp() override {
    BlockCleanupTest::SetUp();
    // Set up a TestChromeBrowserState instance.
    TestChromeBrowserState::Builder test_cbs_builder;

    ASSERT_TRUE(state_dir_.CreateUniqueTempDir());
    test_cbs_builder.SetPath(state_dir_.GetPath());

    test_cbs_builder.AddTestingFactory(
        IOSChromeTabRestoreServiceFactory::GetInstance(),
        IOSChromeTabRestoreServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        IOSChromeLargeIconServiceFactory::GetInstance(),
        IOSChromeLargeIconServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        IOSChromeFaviconLoaderFactory::GetInstance(),
        IOSChromeFaviconLoaderFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::FaviconServiceFactory::GetInstance(),
        ios::FaviconServiceFactory::GetDefaultFactory());

    chrome_browser_state_ = test_cbs_builder.Build();
    ASSERT_TRUE(chrome_browser_state_->CreateHistoryService(true));

    // Set up mock TabModel.
    id tabModel = [OCMockObject niceMockForClass:[TabModel class]];
    OCMStub([tabModel webStateList]).andReturn(&web_state_list_);
    OCMStub([tabModel browserState])
        .andReturn(
            // As OCMock compare types as string, the cast is required otherwise
            // it will complain that the value has an incompatible type.
            static_cast<ChromeBrowserState*>(chrome_browser_state_.get()));

    // Enable web usage for the mock TabModel's WebStateList.
    WebStateListWebUsageEnabler* enabler =
        WebStateListWebUsageEnablerFactory::GetInstance()->GetForBrowserState(
            chrome_browser_state_.get());
    enabler->SetWebStateList([tabModel webStateList]);
    enabler->SetWebUsageEnabled(true);

    id passKitController =
        [OCMockObject niceMockForClass:[PKAddPassesViewController class]];
    passKitViewController_ = passKitController;

    bvcHelper_ = [[BrowserViewControllerHelper alloc] init];

    // Set up a stub dependency factory.
    id factory = [OCMockObject
        mockForClass:[BrowserViewControllerDependencyFactory class]];
    [[[factory stub] andReturn:bvcHelper_] newBrowserViewControllerHelper];

    tabModel_ = tabModel;
    dependencyFactory_ = factory;
    command_dispatcher_ = [[CommandDispatcher alloc] init];
    id mockPageInfoCommandHandler =
        OCMProtocolMock(@protocol(PageInfoCommands));
    [command_dispatcher_ startDispatchingToTarget:mockPageInfoCommandHandler
                                      forProtocol:@protocol(PageInfoCommands)];
    id mockApplicationCommandHandler =
        OCMProtocolMock(@protocol(ApplicationCommands));

    [[tabModel stub] closeAllTabs];

    browser_ =
        std::make_unique<TestBrowser>(chrome_browser_state_.get(), tabModel_);
    SessionRestorationBrowserAgent::CreateForBrowser(
        browser_.get(), [[TestSessionService alloc] init]);

    // Create three web states.
    for (int i = 0; i < 3; i++) {
      web::WebState::CreateParams params(chrome_browser_state_.get());
      std::unique_ptr<web::WebState> webState = web::WebState::Create(params);
      AttachTabHelpers(webState.get(), NO);
      tabModel_.webStateList->InsertWebState(0, std::move(webState), 0,
                                             WebStateOpener());
      tabModel_.webStateList->ActivateWebStateAt(0);
    }

    // Load TemplateURLService.
    TemplateURLService* template_url_service =
        ios::TemplateURLServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
    template_url_service->Load();

    // Instantiate the BVC.
    bvc_ = [[BrowserViewController alloc]
                       initWithBrowser:browser_.get()
                     dependencyFactory:factory
            applicationCommandEndpoint:mockApplicationCommandHandler
           browsingDataCommandEndpoint:nil
                     commandDispatcher:command_dispatcher_
        browserContainerViewController:[[BrowserContainerViewController alloc]
                                           init]];

    // Force the view to load.
    UIWindow* window = [[UIWindow alloc] initWithFrame:CGRectZero];
    [window addSubview:[bvc_ view]];
    window_ = window;
  }

  void TearDown() override {
    [[bvc_ view] removeFromSuperview];
    [bvc_ shutdown];

    // Cleanup to avoid debugger crash in non empty observer lists.
    WebStateList* web_state_list = tabModel_.webStateList;
    web_state_list->CloseAllWebStates(
        WebStateList::ClosingFlags::CLOSE_NO_FLAGS);

    BlockCleanupTest::TearDown();
  }

  web::WebState* ActiveWebState() {
    return tabModel_.webStateList->GetActiveWebState();
  }

  MOCK_METHOD0(OnCompletionCalled, void());

  // A state directory that outlives |task_environment_| is needed because
  // CreateHistoryService/CreateBookmarkModel use the directory to host
  // databases. See https://crbug.com/546640 for more details.
  base::ScopedTempDir state_dir_;

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  TabModel* tabModel_;
  std::unique_ptr<Browser> browser_;
  BrowserViewControllerHelper* bvcHelper_;
  PKAddPassesViewController* passKitViewController_;
  OCMockObject* dependencyFactory_;
  CommandDispatcher* command_dispatcher_;
  BrowserViewController* bvc_;
  UIWindow* window_;
};

TEST_F(BrowserViewControllerTest, TestWebStateSelected) {
  [bvc_ webStateSelected:ActiveWebState() notifyToolbar:YES];
  EXPECT_EQ([ActiveWebState()->GetView() superview], [bvc_ contentArea]);
  EXPECT_TRUE(ActiveWebState()->IsVisible());
}

// TODO(altse): Needs a testing |Profile| that implements AutocompleteClassifier
//             before enabling again.
TEST_F(BrowserViewControllerTest, DISABLED_TestShieldWasTapped) {
  [bvc_.dispatcher focusOmnibox];
  EXPECT_TRUE([[bvc_ typingShield] superview] != nil);
  EXPECT_FALSE([[bvc_ typingShield] isHidden]);
  [bvc_ shieldWasTapped:nil];
  EXPECT_TRUE([[bvc_ typingShield] superview] == nil);
  EXPECT_TRUE([[bvc_ typingShield] isHidden]);
}

// Verifies that editing the omnimbox while the page is loading will stop the
// load on a handset, but not stop the load on a tablet.
TEST_F(BrowserViewControllerTest,
       TestLocationBarBeganEdit_whenPageLoadIsInProgress) {
  // Have the TestLocationBarModel indicate that a page load is in progress.
  id partialMock = OCMPartialMock(bvcHelper_);
  OCMExpect([partialMock isToolbarLoading:static_cast<web::WebState*>(
                                              [OCMArg anyPointer])])
      .andReturn(YES);

  // The tab should stop loading on iPhones.
  [bvc_ locationBarBeganEdit];
  if (!IsIPadIdiom())
    EXPECT_FALSE(ActiveWebState()->IsLoading());
}

TEST_F(BrowserViewControllerTest, TestClearPresentedState) {
  EXPECT_CALL(*this, OnCompletionCalled());
  [bvc_
      clearPresentedStateWithCompletion:^{
        this->OnCompletionCalled();
      }
                         dismissOmnibox:YES];
}

// Verifies the the next/previous tab commands from the keyboard work OK.
TEST_F(BrowserViewControllerTest, TestFocusNextPrevious) {
  // Add more web states.
  WebStateList* web_state_list = tabModel_.webStateList;
  // This test assumes there are exactly three web states in the list.
  ASSERT_EQ(web_state_list->count(), 3);

  ASSERT_TRUE([bvc_ conformsToProtocol:@protocol(KeyCommandsPlumbing)]);

  id<KeyCommandsPlumbing> keyHandler =
      static_cast<id<KeyCommandsPlumbing>>(bvc_);

  [keyHandler focusNextTab];
  EXPECT_EQ(web_state_list->active_index(), 1);
  [keyHandler focusNextTab];
  EXPECT_EQ(web_state_list->active_index(), 2);
  [keyHandler focusNextTab];
  EXPECT_EQ(web_state_list->active_index(), 0);
  [keyHandler focusPreviousTab];
  EXPECT_EQ(web_state_list->active_index(), 2);
  [keyHandler focusPreviousTab];
  EXPECT_EQ(web_state_list->active_index(), 1);
  [keyHandler focusPreviousTab];
  EXPECT_EQ(web_state_list->active_index(), 0);
}

// Tests that WebState::WasShown() and WebState::WasHidden() is properly called
// for WebState activations in the BrowserViewController's WebStateList.
TEST_F(BrowserViewControllerTest, UpdateWebStateVisibility) {
  WebStateList* web_state_list = tabModel_.webStateList;
  ASSERT_EQ(3, web_state_list->count());

  // Activate each WebState in the list and check the visibility.
  web_state_list->ActivateWebStateAt(0);
  EXPECT_EQ(web_state_list->GetWebStateAt(0)->IsVisible(), true);
  EXPECT_EQ(web_state_list->GetWebStateAt(1)->IsVisible(), false);
  EXPECT_EQ(web_state_list->GetWebStateAt(2)->IsVisible(), false);
  web_state_list->ActivateWebStateAt(1);
  EXPECT_EQ(web_state_list->GetWebStateAt(0)->IsVisible(), false);
  EXPECT_EQ(web_state_list->GetWebStateAt(1)->IsVisible(), true);
  EXPECT_EQ(web_state_list->GetWebStateAt(2)->IsVisible(), false);
  web_state_list->ActivateWebStateAt(2);
  EXPECT_EQ(web_state_list->GetWebStateAt(0)->IsVisible(), false);
  EXPECT_EQ(web_state_list->GetWebStateAt(1)->IsVisible(), false);
  EXPECT_EQ(web_state_list->GetWebStateAt(2)->IsVisible(), true);
}

}  // namespace
