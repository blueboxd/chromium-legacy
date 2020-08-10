// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"

#import <UIKit/UIKit.h>

#include "base/values.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ui/activity_services/activity_params.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_positioner.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_presentation.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/generate_qr_code_command.h"
#import "ios/chrome/browser/ui/commands/qr_generation_commands.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class MockTestWebState : public web::TestWebState {
 public:
  MockTestWebState() : web::TestWebState() {
    SetNavigationManager(std::make_unique<web::TestNavigationManager>());
  }

  MOCK_METHOD2(ExecuteJavaScript,
               void(const base::string16&, JavaScriptResultCallback));
};

// Test fixture for testing SharingCoordinator.
class SharingCoordinatorTest : public PlatformTest {
 protected:
  SharingCoordinatorTest()
      : base_view_controller_([[UIViewController alloc] init]),
        browser_(std::make_unique<TestBrowser>()),
        fake_origin_view_([[UIView alloc] init]),
        test_scenario_(ActivityScenario::TabShareButton) {
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
  }

  void AppendNewWebState(std::unique_ptr<web::TestWebState> web_state) {
    browser_->GetWebStateList()->InsertWebState(
        WebStateList::kInvalidIndex, std::move(web_state),
        WebStateList::INSERT_ACTIVATE, WebStateOpener());
  }

  web::WebTaskEnvironment task_environment_;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* base_view_controller_;
  std::unique_ptr<TestBrowser> browser_;
  UIView* fake_origin_view_;
  ActivityScenario test_scenario_;
};

// Tests that the start method shares the current page and ends up presenting
// a UIActivityViewController.
TEST_F(SharingCoordinatorTest, Start_ShareCurrentPage) {
  // Create a test web state.
  GURL test_url = GURL("https://example.com");
  base::Value url_value = base::Value(test_url.spec());
  auto test_web_state = std::make_unique<MockTestWebState>();
  test_web_state->SetCurrentURL(test_url);
  test_web_state->SetBrowserState(browser_->GetBrowserState());

  EXPECT_CALL(*test_web_state, ExecuteJavaScript(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&](const base::string16& javascript,
              base::OnceCallback<void(const base::Value*)> callback) {
            std::move(callback).Run(&url_value);
          }));

  AppendNewWebState(std::move(test_web_state));

  ActivityParams* params =
      [[ActivityParams alloc] initWithScenario:test_scenario_];

  SharingCoordinator* coordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                          params:params
                      originView:fake_origin_view_];

  // Pointer to allow us to grab the VC instance in our validation callback.
  __block UIActivityViewController* activityViewController;

  id vc_partial_mock = OCMPartialMock(base_view_controller_);
  [[vc_partial_mock expect]
      presentViewController:[OCMArg checkWithBlock:^BOOL(
                                        UIViewController* viewController) {
        if ([viewController isKindOfClass:[UIActivityViewController class]]) {
          activityViewController = (UIActivityViewController*)viewController;
          return YES;
        }
        return NO;
      }]
                   animated:YES
                 completion:nil];

  [coordinator start];

  [vc_partial_mock verify];

  // Verify that the positioning is correct.
  auto activityHandler =
      static_cast<id<ActivityServicePositioner, ActivityServicePresentation>>(
          coordinator);
  EXPECT_EQ(fake_origin_view_, activityHandler.shareButtonView);

  // Verify that the presentation protocol works too.
  id activity_vc_partial_mock = OCMPartialMock(activityViewController);
  [[activity_vc_partial_mock expect] dismissViewControllerAnimated:YES
                                                        completion:nil];

  [activityHandler activityServiceDidEndPresenting];

  [activity_vc_partial_mock verify];
}

// Tests that the coordinator handles the QRGenerationCommands protocol.
TEST_F(SharingCoordinatorTest, GenerateQRCode) {
  ActivityParams* params =
      [[ActivityParams alloc] initWithScenario:test_scenario_];
  SharingCoordinator* coordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                          params:params
                      originView:fake_origin_view_];

  id vc_partial_mock = OCMPartialMock(base_view_controller_);
  [[vc_partial_mock expect] presentViewController:[OCMArg any]
                                         animated:YES
                                       completion:nil];

  auto handler = static_cast<id<QRGenerationCommands>>(coordinator);
  [handler generateQRCode:[[GenerateQRCodeCommand alloc]
                              initWithURL:GURL("https://example.com")
                                    title:@"Some Title"]];

  [vc_partial_mock verify];

  [[vc_partial_mock expect] dismissViewControllerAnimated:YES completion:nil];

  [handler hideQRCode];

  [vc_partial_mock verify];
}

// Tests that the start method shares the given URL and ends up presenting
// a UIActivityViewController.
TEST_F(SharingCoordinatorTest, Start_ShareURL) {
  GURL testURL = GURL("https://example.com");
  NSString* testTitle = @"Some title";
  ActivityParams* params = [[ActivityParams alloc] initWithURL:testURL
                                                         title:testTitle
                                                      scenario:test_scenario_];
  SharingCoordinator* coordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                          params:params
                      originView:fake_origin_view_];

  // Pointer to allow us to grab the VC instance in our validation callback.
  __block UIActivityViewController* activityViewController;

  id vc_partial_mock = OCMPartialMock(base_view_controller_);
  [[vc_partial_mock expect]
      presentViewController:[OCMArg checkWithBlock:^BOOL(
                                        UIViewController* viewController) {
        if ([viewController isKindOfClass:[UIActivityViewController class]]) {
          activityViewController = (UIActivityViewController*)viewController;
          return YES;
        }
        return NO;
      }]
                   animated:YES
                 completion:nil];

  [coordinator start];

  [vc_partial_mock verify];
}
