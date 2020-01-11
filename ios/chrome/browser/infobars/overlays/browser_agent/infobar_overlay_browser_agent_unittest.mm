// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/infobar_overlay_browser_agent.h"

#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/infobar_banner_overlay_request_callback_installer.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/test/mock_infobar_interaction_handler.h"
#import "ios/chrome/browser/infobars/test/fake_infobar_ios.h"
#import "ios/chrome/browser/main/browser_user_data.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/infobar_banner/infobar_banner_overlay_responses.h"
#include "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#include "ios/chrome/browser/overlays/public/overlay_response.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_presentation_context.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_request_callback_installer.h"
#include "ios/chrome/browser/overlays/test/overlay_test_macros.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for InfobarOverlayBrowserAgent.
class InfobarOverlayBrowserAgentTest : public PlatformTest {
 public:
  InfobarOverlayBrowserAgentTest()
      : browser_state_(browser_state_builder_.Build()),
        web_state_list_(&web_state_list_delegate_),
        browser_(browser_state_.get(), &web_state_list_) {
    // Add an activated WebState into whose queues infobar OverlayRequests will
    // be added.
    std::unique_ptr<web::WebState> web_state =
        std::make_unique<web::TestWebState>();
    web_state_ = web_state.get();
    web_state_list_.InsertWebState(/*index=*/0, std::move(web_state),
                                   WebStateList::INSERT_ACTIVATE,
                                   WebStateOpener());
    // Set up the infobar banner OverlayPresenter's presentation context so that
    // presentation can be faked.
    banner_presenter()->SetPresentationContext(&banner_presentation_context_);
    // Set up the browser agent.
    InfobarOverlayBrowserAgent::CreateForBrowser(&browser_);
    std::unique_ptr<MockInfobarInteractionHandler> interaction_handler =
        std::make_unique<MockInfobarInteractionHandler>();
    mock_interaction_handler_ = interaction_handler.get();
    browser_agent()->SetInfobarInteractionHandler(
        InfobarType::kInfobarTypeConfirm, std::move(interaction_handler));
  }
  ~InfobarOverlayBrowserAgentTest() override {
    banner_presenter()->SetPresentationContext(nullptr);
  }

  InfobarOverlayBrowserAgent* browser_agent() {
    return InfobarOverlayBrowserAgent::FromBrowser(&browser_);
  }
  OverlayPresenter* banner_presenter() {
    return OverlayPresenter::FromBrowser(&browser_,
                                         OverlayModality::kInfobarBanner);
  }
  OverlayRequestQueue* banner_queue() const {
    return OverlayRequestQueue::FromWebState(web_state_,
                                             OverlayModality::kInfobarBanner);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  TestChromeBrowserState::Builder browser_state_builder_;
  std::unique_ptr<ios::ChromeBrowserState> browser_state_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  web::WebState* web_state_ = nullptr;
  TestBrowser browser_;
  FakeOverlayPresentationContext banner_presentation_context_;
  FakeInfobarIOS infobar_;
  MockInfobarInteractionHandler* mock_interaction_handler_ = nullptr;
};

// Tests that the interaction handler is notified of banner presentation and
// dismissal.  The rest of InfobarBannerInteractionHandler's interface is tested
// by InfobarBannerOverlayRequestCallbackInstallerTest.
TEST_F(InfobarOverlayBrowserAgentTest, BannerPresentation) {
  // Add an infobar request to the banner modality, expecting
  // InfobarBannerInteractionHandler::BannerVisibilityChanged() to be called.
  std::unique_ptr<OverlayRequest> added_request =
      OverlayRequest::CreateWithConfig<InfobarOverlayRequestConfig>(&infobar_);
  OverlayRequest* request = added_request.get();
  MockInfobarBannerInteractionHandler& mock_banner_handler =
      *mock_interaction_handler_->mock_banner_handler();
  EXPECT_CALL(mock_banner_handler,
              BannerVisibilityChanged(&infobar_, /*visible=*/true));
  banner_queue()->AddRequest(std::move(added_request));
  // Simulate dismissal of the request's UI, expecting
  // InfobarBannerInteractionHandler::BannerVisibilityChanged() to be called.
  EXPECT_CALL(mock_banner_handler,
              BannerVisibilityChanged(&infobar_, /*visible=*/false));
  banner_presentation_context_.SimulateDismissalForRequest(
      request, OverlayDismissalReason::kUserInteraction);
}
