// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/app_launcher/app_launcher_coordinator.h"

#import <UIKit/UIKit.h>

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/app_launcher/app_launcher_tab_helper.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/public/web_content_area/app_launcher_alert_overlay.h"
#import "ios/chrome/browser/ui/dialogs/dialog_features.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for AppLauncherCoordinator class.
class AppLauncherCoordinatorTest : public PlatformTest {
 protected:
  AppLauncherCoordinatorTest() {
    base_view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
    coordinator_ = [[AppLauncherCoordinator alloc]
        initWithBaseViewController:base_view_controller_];
    application_ = OCMClassMock([UIApplication class]);
    OCMStub([application_ sharedApplication]).andReturn(application_);
    AppLauncherTabHelper::CreateForWebState(&web_state_, nil, nil);
  }
  ~AppLauncherCoordinatorTest() override {
    [application_ stopMocking];
    OverlayRequestQueue::FromWebState(&web_state_,
                                      OverlayModality::kWebContentArea)
        ->CancelAllRequests();
  }

  AppLauncherTabHelper* tab_helper() {
    return AppLauncherTabHelper::FromWebState(&web_state_);
  }

  bool IsShowingDialog(bool is_repeated_request) {
    if (base::FeatureList::IsEnabled(dialogs::kNonModalDialogs)) {
      OverlayRequest* request =
          OverlayRequestQueue::FromWebState(&web_state_,
                                            OverlayModality::kWebContentArea)
              ->front_request();
      if (!request)
        return false;
      AppLauncherAlertOverlayRequestConfig* config =
          request->GetConfig<AppLauncherAlertOverlayRequestConfig>();
      return config && config->is_repeated_request() == is_repeated_request;
    } else {
      UIAlertController* alert_controller =
          base::mac::ObjCCastStrict<UIAlertController>(
              base_view_controller_.presentedViewController);
      NSString* message =
          is_repeated_request
              ? l10n_util::GetNSString(IDS_IOS_OPEN_REPEATEDLY_ANOTHER_APP)
              : l10n_util::GetNSString(IDS_IOS_OPEN_IN_ANOTHER_APP);
      return alert_controller.message == message;
    }
  }

  web::TestWebState web_state_;
  UIViewController* base_view_controller_ = nil;
  ScopedKeyWindow scoped_key_window_;
  AppLauncherCoordinator* coordinator_ = nil;
  id application_ = nil;
};


// Tests that an itunes URL shows an alert.
TEST_F(AppLauncherCoordinatorTest, ItmsUrlShowsAlert) {
  BOOL app_exists = [coordinator_ appLauncherTabHelper:tab_helper()
                                      launchAppWithURL:GURL("itms://1234")
                                        linkTransition:NO];
  EXPECT_TRUE(app_exists);
  EXPECT_TRUE(IsShowingDialog(/*is_repeated_request=*/false));
}

// Tests that in the new AppLauncher, an app URL attempts to launch the
// application.
TEST_F(AppLauncherCoordinatorTest, AppUrlLaunchesApp) {
  OCMExpect([application_ openURL:[NSURL URLWithString:@"some-app://1234"]
                          options:@{}
                completionHandler:nil]);
  [coordinator_ appLauncherTabHelper:tab_helper()
                    launchAppWithURL:GURL("some-app://1234")
                      linkTransition:YES];
  [application_ verify];
}

// Tests that in the new AppLauncher, an app URL shows a prompt if there was no
// link transition.
TEST_F(AppLauncherCoordinatorTest, AppUrlShowsPrompt) {
  [coordinator_ appLauncherTabHelper:tab_helper()
                    launchAppWithURL:GURL("some-app://1234")
                      linkTransition:NO];
  EXPECT_TRUE(IsShowingDialog(/*is_repeated_request=*/false));
}
