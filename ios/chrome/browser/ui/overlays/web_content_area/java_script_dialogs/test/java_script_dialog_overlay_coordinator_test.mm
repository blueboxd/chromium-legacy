// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/test/java_script_dialog_overlay_coordinator_test.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_view_controller.h"
#import "ios/chrome/browser/ui/overlays/overlay_coordinator_factory+initialization.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_alert_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_confirmation_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_prompt_overlay_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

JavaScriptDialogOverlayCoordinatorTest::JavaScriptDialogOverlayCoordinatorTest()
    : PlatformTest(),
      web_state_list_(&web_state_list_delegate_),
      base_view_controller([[UIViewController alloc] init]) {
  // Set up the TestBrowser.
  TestChromeBrowserState::Builder browser_state_builder;
  chrome_browser_state_ = browser_state_builder.Build();
  browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get(),
                                           &web_state_list_);
  // Set up the view hierarchy for testing.
  base_view_controller.definesPresentationContext = YES;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];
}

JavaScriptDialogOverlayCoordinatorTest::
    ~JavaScriptDialogOverlayCoordinatorTest() = default;

void JavaScriptDialogOverlayCoordinatorTest::SetRequest(
    std::unique_ptr<OverlayRequest> request) {
  DCHECK(!request_);
  DCHECK(request);
  request_ = std::move(request);
  NSArray<Class>* coordinator_classes =
      @ [[JavaScriptAlertOverlayCoordinator class],
         [JavaScriptConfirmationOverlayCoordinator class],
         [JavaScriptPromptOverlayCoordinator class]];
  OverlayRequestCoordinatorFactory* factory =
      [[OverlayRequestCoordinatorFactory alloc]
                                    initWithBrowser:browser_.get()
          supportedOverlayRequestCoordinatorClasses:coordinator_classes];
  coordinator_ = [factory newCoordinatorForRequest:request_.get()
                                          delegate:&delegate_
                                baseViewController:base_view_controller];
}

void JavaScriptDialogOverlayCoordinatorTest::StartDialogCoordinator() {
  DCHECK(coordinator_);
  [coordinator_ startAnimated:NO];
}

void JavaScriptDialogOverlayCoordinatorTest::StopDialogCoordinator() {
  DCHECK(coordinator_);
  [coordinator_ stopAnimated:NO];
}

UIViewController*
JavaScriptDialogOverlayCoordinatorTest::GetAlertViewController() {
  return coordinator_.viewController;
}
