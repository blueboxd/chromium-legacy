// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/overlay_container_coordinator.h"

#import "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_alert_overlay.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The modality to use for tests.
const OverlayModality kModality = OverlayModality::kWebContentArea;
// Adds a supported request to |web_state|'s queue at kModality.  A JavaScript
// alert request is added since the test modality is kWebContentArea.
void AddRequest(web::WebState* web_state) {
  OverlayRequestQueue* queue =
      OverlayRequestQueue::FromWebState(web_state, kModality);
  const GURL kUrl("http://chromium.test");
  JavaScriptDialogSource source(web_state, kUrl, true);
  const std::string kMessage("message");
  queue->AddRequest(
      OverlayRequest::CreateWithConfig<JavaScriptAlertOverlayRequestConfig>(
          source, kMessage));
}
}  // namespace

// Test fixture for OverlayContainerCoordinator.
class OverlayContainerCoordinatorTest : public PlatformTest {
 protected:
  OverlayContainerCoordinatorTest()
      : base_view_controller_([[UIViewController alloc] init]),
        coordinator_([[OverlayContainerCoordinator alloc]
            initWithBaseViewController:base_view_controller_
                               browser:&browser_
                              modality:kModality]) {
    // Add a WebState to the Browser.
    WebStateList* web_state_list = browser_.GetWebStateList();
    web_state_list->InsertWebState(0, std::make_unique<web::TestWebState>(),
                                   WebStateList::INSERT_ACTIVATE,
                                   WebStateOpener());
    web_state_ = web_state_list->GetActiveWebState();

    // Set up the view hierarchy so that overlay presentation can occur.
    scoped_window_.Get().rootViewController = base_view_controller_;
    [coordinator_ start];
  }
  ~OverlayContainerCoordinatorTest() override { [coordinator_ stop]; }

  ScopedKeyWindow scoped_window_;
  web::WebTaskEnvironment task_environment_;
  TestBrowser browser_;
  web::WebState* web_state_ = nullptr;
  UIViewController* base_view_controller_ = nil;
  OverlayContainerCoordinator* coordinator_ = nil;
};

// Tests that the FullscreenController is disabled when an overlay presented.
TEST_F(OverlayContainerCoordinatorTest, DisableFullscreen) {
  FullscreenController* controller =
      FullscreenController::FromBrowserState(browser_.GetBrowserState());
  ASSERT_TRUE(controller->IsEnabled());
  AddRequest(web_state_);
  ASSERT_FALSE(controller->IsEnabled());
}
