// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/infobar_overlay_request_cancel_handler.h"

#include "components/infobars/core/infobar.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_user_data.h"
#import "ios/chrome/browser/ui/infobars/test_infobar_delegate.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;
using infobars::InfoBarDelegate;
using infobars::InfoBarManager;

// Test fixture for InfobarOverlayRequestCancelHandler.
class InfobarOverlayRequestCancelHandlerTest : public PlatformTest {
 public:
  InfobarOverlayRequestCancelHandlerTest() : PlatformTest() {
    // Set up WebState and InfoBarManager.
    web_state_.SetNavigationManager(
        std::make_unique<web::TestNavigationManager>());
    InfoBarManagerImpl::CreateForWebState(&web_state_);
    // Create a test InfoBar and add it to the manager.
    std::unique_ptr<InfoBarDelegate> delegate =
        std::make_unique<TestInfoBarDelegate>(nil);
    std::unique_ptr<InfoBar> infobar =
        std::make_unique<InfoBar>(std::move(delegate));
    infobar_ = infobar.get();
    manager()->AddInfoBar(std::move(infobar));
    // Create a fake OverlayRequest and add it to the infobar banner queue using
    // an infobar cancel handler.
    std::unique_ptr<OverlayRequest> request =
        OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr);
    std::unique_ptr<OverlayRequestCancelHandler> cancel_handler =
        std::make_unique<InfobarOverlayRequestCancelHandler>(request.get(),
                                                             queue(), infobar_);
    queue()->AddRequest(std::move(request), std::move(cancel_handler));
  }

  OverlayRequestQueue* queue() {
    return OverlayRequestQueue::FromWebState(&web_state_,
                                             OverlayModality::kInfobarBanner);
  }
  InfoBarManager* manager() {
    return InfoBarManagerImpl::FromWebState(&web_state_);
  }
  InfoBar* infobar() { return infobar_; }

 private:
  web::TestWebState web_state_;
  InfoBar* infobar_ = nullptr;
};

// Tests that the request is cancelled when its corresponding InfoBar is removed
// from its InfoBarManager.
TEST_F(InfobarOverlayRequestCancelHandlerTest, CancelForInfobarRemoval) {
  ASSERT_TRUE(queue()->front_request());
  // Remove the InfoBar and verify that the request has been removed from the
  // queue.
  manager()->RemoveInfoBar(infobar());
  EXPECT_FALSE(queue()->front_request());
}
