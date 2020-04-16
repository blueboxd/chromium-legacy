// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/common/alerts/alert_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/common/alerts/alert_overlay_coordinator+alert_mediator_creation.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/test/overlay_test_macros.h"
#import "ios/chrome/browser/ui/alert_view/alert_action.h"
#import "ios/chrome/browser/ui/alert_view/alert_view_controller.h"
#import "ios/chrome/browser/ui/overlays/common/alerts/test/fake_alert_overlay_mediator.h"
#include "ios/chrome/browser/ui/overlays/test/fake_overlay_request_coordinator_delegate.h"
#include "ios/chrome/test/scoped_key_window.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Fake request config type for use in tests.
DEFINE_TEST_OVERLAY_REQUEST_CONFIG(FakeAlert);
}

#pragma mark - FakeAlertOverlayCoordinator

@interface FakeAlertOverlayCoordinator : AlertOverlayCoordinator
@property(nonatomic, strong) FakeAlertOverlayMediator* fakeMediator;
@end

@implementation FakeAlertOverlayCoordinator
+ (const OverlayRequestSupport*)requestSupport {
  return FakeAlert::RequestSupport();
}
@end

@implementation FakeAlertOverlayCoordinator (AlertMediatorCreation)
- (AlertOverlayMediator*)newMediator {
  return self.fakeMediator;
}
@end

#pragma mark - AlertOverlayCoordinatorTest

// Test fixture for AlertOverlayCoordinator.
class AlertOverlayCoordinatorTest : public PlatformTest {
 public:
  AlertOverlayCoordinatorTest()
      : root_view_controller_([[UIViewController alloc] init]),
        request_(OverlayRequest::CreateWithConfig<FakeAlert>()),
        mediator_(
            [[FakeAlertOverlayMediator alloc] initWithRequest:request_.get()]),
        coordinator_([[FakeAlertOverlayCoordinator alloc]
            initWithBaseViewController:root_view_controller_
                               browser:&browser_
                               request:request_.get()
                              delegate:&delegate_]) {
    scoped_window_.Get().rootViewController = root_view_controller_;
    // Set up the fake mediator and provide it to the coordinator.
    mediator_.alertTitle = @"title";
    mediator_.alertMessage = @"message";
    mediator_.alertActions =
        @[ [AlertAction actionWithTitle:@"OK"
                                  style:UIAlertActionStyleDefault
                                handler:nil] ];
    coordinator_.fakeMediator = mediator_;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  TestBrowser browser_;
  ScopedKeyWindow scoped_window_;
  UIViewController* root_view_controller_ = nil;
  std::unique_ptr<OverlayRequest> request_;
  FakeOverlayRequestCoordinatorDelegate delegate_;
  FakeAlertOverlayMediator* mediator_ = nil;
  FakeAlertOverlayCoordinator* coordinator_ = nil;
};

// Tests that the coordinator creates an alert view, presents it within the
// base UIViewController's hierarchy, and sets it up using its mediator.
TEST_F(AlertOverlayCoordinatorTest, ViewSetup) {
  [coordinator_ startAnimated:NO];
  AlertViewController* view_controller =
      base::mac::ObjCCast<AlertViewController>(coordinator_.viewController);
  EXPECT_TRUE(view_controller);
  EXPECT_EQ(view_controller.parentViewController, root_view_controller_);
  EXPECT_TRUE(
      [view_controller.view isDescendantOfView:root_view_controller_.view]);
  EXPECT_TRUE(delegate_.HasUIBeenPresented(request_.get()));
  EXPECT_EQ(mediator_.consumer, view_controller);
  [coordinator_ stopAnimated:NO];
  EXPECT_TRUE(delegate_.HasUIBeenDismissed(request_.get()));
}
