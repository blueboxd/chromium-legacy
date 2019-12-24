// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/infobar_badge_browser_agent.h"

#import <Foundation/Foundation.h>
#include <map>

#include "base/test/scoped_feature_list.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar_badge_tab_helper.h"
#include "ios/chrome/browser/infobars/infobar_badge_tab_helper_delegate.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/overlays/fake_infobar_overlay_request_factory.h"
#import "ios/chrome/browser/infobars/test/fake_infobar_badge_tab_helper_delegate.h"
#import "ios/chrome/browser/infobars/test/fake_infobar_ios.h"
#import "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#include "ios/chrome/browser/overlays/public/overlay_response.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_presentation_context.h"
#import "ios/chrome/browser/ui/badges/badge_item.h"
#import "ios/chrome/browser/ui/badges/badge_type.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/infobars/test/fake_infobar_ui_delegate.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;

namespace {
// The InfobarType to use for the test.
const InfobarType kInfobarType = InfobarType::kInfobarTypePasswordSave;
// The corresponding BadgeType for kInfobarType.
const BadgeType kBadgeType = BadgeType::kBadgeTypePasswordSave;
}  // namespace

// Test fixture for InfobarBadgeBrowserAgent.
class InfobarBadgeBrowserAgentTest : public PlatformTest {
 public:
  InfobarBadgeBrowserAgentTest()
      : web_state_list_(&web_state_list_delegate_),
        tab_helper_delegate_([[FakeInfobarTabHelperDelegate alloc] init]) {
    // Enable the UI reboot feature.
    feature_list_.InitAndEnableFeature(kInfobarUIReboot);
    // Create the Browser and set up the browser agent.
    TestChromeBrowserState::Builder builder;
    browser_state_ = builder.Build();
    browser_ =
        std::make_unique<TestBrowser>(browser_state_.get(), &web_state_list_);
    InfobarBadgeBrowserAgent::CreateForBrowser(browser_.get());
    // Create a WebState and add it to the Browser.
    std::unique_ptr<web::TestWebState> passed_web_state =
        std::make_unique<web::TestWebState>();
    passed_web_state->SetNavigationManager(
        std::make_unique<web::TestNavigationManager>());
    web_state_ = passed_web_state.get();
    web_state_list_.InsertWebState(0, std::move(passed_web_state),
                                   WebStateList::INSERT_ACTIVATE,
                                   WebStateOpener());
    // Set up the WebState's InfoBarManager and InfobarBadgeTabHelper.
    InfoBarManagerImpl::CreateForWebState(web_state_);
    InfobarBadgeTabHelper::CreateForWebState(web_state_);
    InfobarBadgeTabHelper::FromWebState(web_state_)
        ->SetDelegate(tab_helper_delegate_);
    // Set up the OverlayPresenter for OverlayModality::kInfobarBanner.
    presenter()->SetPresentationContext(&presentation_context_);
  }

  ~InfobarBadgeBrowserAgentTest() override {
    presenter()->SetPresentationContext(nullptr);
  }

  // Adds an InfoBar with |type| to the manager and returns the added infobar.
  InfoBar* AddInfobar() {
    std::unique_ptr<FakeInfobarIOS> passed_infobar =
        std::make_unique<FakeInfobarIOS>();
    InfoBar* infobar = passed_infobar.get();
    passed_infobar->fake_ui_delegate().infobarType = kInfobarType;
    passed_infobar->fake_ui_delegate().hasBadge = YES;
    InfoBarManagerImpl::FromWebState(web_state_)
        ->AddInfoBar(std::move(passed_infobar));
    return infobar;
  }

  // Adds a fake banner OverlayRequest for |infobar|, adds it to queue(), and
  // returns the added request.
  OverlayRequest* AddOverlayRequest(InfoBar* infobar) {
    FakeInfobarOverlayRequestFactory factory;
    std::unique_ptr<OverlayRequest> passed_request =
        factory.CreateInfobarRequest(infobar, InfobarOverlayType::kBanner);
    OverlayRequest* request = passed_request.get();
    queue()->AddRequest(std::move(passed_request));
    return request;
  }

  OverlayPresenter* presenter() const {
    return OverlayPresenter::FromBrowser(browser_.get(),
                                         OverlayModality::kInfobarBanner);
  }
  OverlayRequestQueue* queue() const {
    return OverlayRequestQueue::FromWebState(web_state_,
                                             OverlayModality::kInfobarBanner);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ios::ChromeBrowserState> browser_state_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  web::WebState* web_state_ = nullptr;
  std::unique_ptr<Browser> browser_;
  FakeOverlayPresentationContext presentation_context_;
  FakeInfobarTabHelperDelegate* tab_helper_delegate_ = nil;
};

// Tests that the browser agent correctly updates the badge state to presented.
TEST_F(InfobarBadgeBrowserAgentTest, InfobarPresented) {
  // Simulate the presentation of an infobar banner overlay and verify that the
  // badge state gets updated to presented.
  InfoBar* infobar = AddInfobar();
  OverlayRequest* request = AddOverlayRequest(infobar);
  id<BadgeItem> badge = [tab_helper_delegate_ itemForBadgeType:kBadgeType];
  ASSERT_TRUE(badge);
  ASSERT_EQ(FakeOverlayPresentationContext::PresentationState::kPresented,
            presentation_context_.GetPresentationState(request));
  EXPECT_TRUE(badge.badgeState & BadgeStatePresented);
}

// Tests that the browser agent correctly updates the badge state to dismissed.
TEST_F(InfobarBadgeBrowserAgentTest, InfobarDismissed) {
  InfoBar* infobar = AddInfobar();
  OverlayRequest* request = AddOverlayRequest(infobar);
  id<BadgeItem> badge = [tab_helper_delegate_ itemForBadgeType:kBadgeType];
  ASSERT_TRUE(badge);
  ASSERT_EQ(FakeOverlayPresentationContext::PresentationState::kPresented,
            presentation_context_.GetPresentationState(request));
  ASSERT_TRUE(badge.badgeState & BadgeStatePresented);

  // Simulate the dismissal of the infobar banner overlay and verify that the
  // badge state gets updated to dismissed.
  presentation_context_.SimulateDismissalForRequest(
      request, OverlayDismissalReason::kUserInteraction);
  EXPECT_FALSE(badge.badgeState & BadgeStatePresented);
}

// TODO(crbug.com/1030357): Add tests for non-banners.
