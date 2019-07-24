// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/ntp_home_mediator.h"

#include <memory>

#include "components/signin/public/identity_manager/identity_manager.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_consumer.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_notification_names.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_test_navigation_manager.h"
#include "ios/chrome/browser/url_loading/test_url_loading_service.h"
#include "ios/chrome/browser/url_loading/url_loading_params.h"
#include "ios/chrome/browser/url_loading/url_loading_service_factory.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/public/provider/chrome/browser/ui/logo_vendor.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@protocol NTPHomeMediatorDispatcher <BrowserCommands, SnackbarCommands>
@end

namespace {
static const int kNumberOfWebStates = 3;
}

class NTPHomeMediatorTest : public PlatformTest {
 public:
  NTPHomeMediatorTest() {
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        IOSChromeContentSuggestionsServiceFactory::GetInstance(),
        IOSChromeContentSuggestionsServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        UrlLoadingServiceFactory::GetInstance(),
        UrlLoadingServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(
            &AuthenticationServiceFake::CreateAuthenticationService));
    chrome_browser_state_ = test_cbs_builder.Build();

    std::unique_ptr<ToolbarTestNavigationManager> navigation_manager =
        std::make_unique<ToolbarTestNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    test_web_state_ = std::make_unique<web::TestWebState>();
    test_web_state_->SetNavigationManager(std::move(navigation_manager));
    web_state_ = test_web_state_.get();
    SetUpWebStateList();
    logo_vendor_ = OCMProtocolMock(@protocol(LogoVendor));
    dispatcher_ = OCMProtocolMock(@protocol(NTPHomeMediatorDispatcher));
    suggestions_view_controller_ =
        OCMClassMock([ContentSuggestionsViewController class]);
    url_loader_ =
        (TestUrlLoadingService*)UrlLoadingServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
    auth_service_ = static_cast<AuthenticationServiceFake*>(
        AuthenticationServiceFactory::GetInstance()->GetForBrowserState(
            chrome_browser_state_.get()));
    identity_manager_ =
        IdentityManagerFactory::GetForBrowserState(chrome_browser_state_.get());
    mediator_ = [[NTPHomeMediator alloc]
        initWithWebStateList:web_state_list_.get()
          templateURLService:ios::TemplateURLServiceFactory::GetForBrowserState(
                                 chrome_browser_state_.get())
           urlLoadingService:url_loader_
                 authService:auth_service_
             identityManager:identity_manager_
                  logoVendor:logo_vendor_];
    mediator_.suggestionsService =
        IOSChromeContentSuggestionsServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
    mediator_.dispatcher = dispatcher_;
    mediator_.suggestionsViewController = suggestions_view_controller_;
    consumer_ = OCMProtocolMock(@protocol(NTPHomeConsumer));
    mediator_.consumer = consumer_;
  }

  // Explicitly disconnect the mediator so there won't be any WebStateList
  // observers when web_state_list_ gets dealloc.
  ~NTPHomeMediatorTest() override { [mediator_ shutdown]; }

 protected:
  void SetUpWebStateList() {
    web_state_list_ = std::make_unique<WebStateList>(&web_state_list_delegate_);
    web_state_list_->InsertWebState(0, std::move(test_web_state_),
                                    WebStateList::INSERT_FORCE_INDEX,
                                    WebStateOpener());
    web_state_list_->ActivateWebStateAt(0);
    for (int i = 1; i < kNumberOfWebStates; i++) {
      auto web_state = std::make_unique<web::TestWebState>();
      GURL url("http://test/" + std::to_string(i));
      web_state->SetCurrentURL(url);
      web_state_list_->InsertWebState(i, std::move(web_state),
                                      WebStateList::INSERT_FORCE_INDEX,
                                      WebStateOpener());
    }
  }

  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  id consumer_;
  id logo_vendor_;
  id dispatcher_;
  id suggestions_view_controller_;
  NTPHomeMediator* mediator_;
  web::TestWebState* web_state_;
  ToolbarTestNavigationManager* navigation_manager_;
  std::unique_ptr<WebStateList> web_state_list_;
  FakeWebStateListDelegate web_state_list_delegate_;
  TestUrlLoadingService* url_loader_;
  AuthenticationServiceFake* auth_service_;
  signin::IdentityManager* identity_manager_;

 private:
  std::unique_ptr<web::TestWebState> test_web_state_;
};

// Tests that the consumer has the right value set up.
TEST_F(NTPHomeMediatorTest, TestConsumerSetup) {
  // Setup.
  navigation_manager_->set_can_go_forward(true);
  navigation_manager_->set_can_go_back(false);

  OCMExpect([consumer_ setTabCount:kNumberOfWebStates]);
  OCMExpect([consumer_ setLogoVendor:logo_vendor_]);
  OCMExpect([consumer_ setCanGoForward:YES]);
  OCMExpect([consumer_ setCanGoBack:NO]);
  OCMExpect([consumer_ setLogoIsShowing:YES]);

  // Action.
  [mediator_ setUp];

  // Tests.
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is notified when the location bar is focused.
TEST_F(NTPHomeMediatorTest, TestConsumerNotificationFocus) {
  // Setup.
  [mediator_ setUp];

  OCMExpect([consumer_ locationBarBecomesFirstResponder]);

  // Action.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kLocationBarBecomesFirstResponderNotification
                    object:nil];

  // Test.
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is notified when the location bar is unfocused.
TEST_F(NTPHomeMediatorTest, TestConsumerNotificationUnfocus) {
  // Setup.
  [mediator_ setUp];

  OCMExpect([consumer_ locationBarResignsFirstResponder]);

  // Action.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kLocationBarResignsFirstResponderNotification
                    object:nil];

  // Test.
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is notified when the number of tab increases.
TEST_F(NTPHomeMediatorTest, TestTabCountInsert) {
  // Setup.
  [mediator_ setUp];

  OCMExpect([consumer_ setTabCount:kNumberOfWebStates + 1]);

  // Action.
  auto web_state = std::make_unique<web::TestWebState>();
  web_state_list_->InsertWebState(1, std::move(web_state),
                                  WebStateList::INSERT_FORCE_INDEX,
                                  WebStateOpener());

  // Test.
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is notified when the number of tab decreases.
TEST_F(NTPHomeMediatorTest, TestTabCountDetach) {
  // Setup.
  [mediator_ setUp];

  OCMExpect([consumer_ setTabCount:kNumberOfWebStates - 1]);

  // Action.
  web_state_list_->DetachWebStateAt(1);

  // Test.
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is notified when the active web state changes.
TEST_F(NTPHomeMediatorTest, TestChangeActiveWebState) {
  // Setup.
  [mediator_ setUp];
  std::unique_ptr<ToolbarTestNavigationManager> navigation_manager =
      std::make_unique<ToolbarTestNavigationManager>();
  ToolbarTestNavigationManager* nav = navigation_manager.get();
  std::unique_ptr<web::TestWebState> web_state =
      std::make_unique<web::TestWebState>();
  web_state->SetNavigationManager(std::move(navigation_manager));
  nav->set_can_go_back(true);
  nav->set_can_go_forward(false);
  web_state_list_->InsertWebState(1, std::move(web_state),
                                  WebStateList::INSERT_FORCE_INDEX,
                                  WebStateOpener());

  OCMExpect([consumer_ setCanGoForward:NO]);
  OCMExpect([consumer_ setCanGoBack:YES]);

  // Action.
  web_state_list_->ActivateWebStateAt(1);

  // Test.
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the command is sent to the dispatcher when opening the Reading
// List.
TEST_F(NTPHomeMediatorTest, TestOpenReadingList) {
  // Setup.
  [mediator_ setUp];
  OCMExpect([dispatcher_ showReadingList]);

  // Action.
  [mediator_ openReadingList];

  // Test.
  EXPECT_OCMOCK_VERIFY(dispatcher_);
}

// Tests that the command is sent to the loader when opening a suggestion.
TEST_F(NTPHomeMediatorTest, TestOpenPage) {
  // Setup.
  [mediator_ setUp];
  GURL url = GURL("http://chromium.org");
  NSIndexPath* indexPath = [NSIndexPath indexPathForItem:0 inSection:0];
  ContentSuggestionsItem* item =
      [[ContentSuggestionsItem alloc] initWithType:0
                                             title:@"test item"
                                               url:url];
  id model = OCMClassMock([CollectionViewModel class]);
  OCMStub([suggestions_view_controller_ collectionViewModel]).andReturn(model);
  OCMStub([model itemAtIndexPath:indexPath]).andReturn(item);

  // Action.
  [mediator_ openPageForItemAtIndexPath:indexPath];

  // Test.
  EXPECT_EQ(url, url_loader_->last_params.web_params.url);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      ui::PAGE_TRANSITION_AUTO_BOOKMARK,
      url_loader_->last_params.web_params.transition_type));
}

// Tests that the command is sent to the loader when opening a most visited.
TEST_F(NTPHomeMediatorTest, TestOpenMostVisited) {
  // Setup.
  [mediator_ setUp];
  GURL url = GURL("http://chromium.org");
  ContentSuggestionsMostVisitedItem* item =
      [[ContentSuggestionsMostVisitedItem alloc] initWithType:0];
  item.URL = url;

  // Action.
  [mediator_ openMostVisitedItem:item atIndex:0];

  // Test.
  EXPECT_EQ(url, url_loader_->last_params.web_params.url);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      ui::PAGE_TRANSITION_AUTO_BOOKMARK,
      url_loader_->last_params.web_params.transition_type));
}
