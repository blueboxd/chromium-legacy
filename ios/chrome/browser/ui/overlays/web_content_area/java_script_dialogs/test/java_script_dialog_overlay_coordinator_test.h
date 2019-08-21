// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOGS_TEST_JAVA_SCRIPT_DIALOG_OVERLAY_COORDINATOR_TEST_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOGS_TEST_JAVA_SCRIPT_DIALOG_OVERLAY_COORDINATOR_TEST_H_

#import <UIKit/UIKit.h>
#include <memory>

#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/ui/overlays/test/fake_overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/test/scoped_key_window.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/platform_test.h"

@class AlertViewController;
class OverlayRequest;
@class OverlayRequestCoordinator;

// Test fixture for testing JavaScriptDialogOverlayCoordinator subclasses.
class JavaScriptDialogOverlayCoordinatorTest : public PlatformTest {
 protected:
  JavaScriptDialogOverlayCoordinatorTest();
  ~JavaScriptDialogOverlayCoordinatorTest() override;

  // Accessors:
  const FakeOverlayRequestCoordinatorDelegate& delegate() const {
    return delegate_;
  }

  // Sets the request for the test.  Setting to a new value will create a
  // coordinator that will show the dialog UI for |request|.
  void SetRequest(std::unique_ptr<OverlayRequest> request);

  // Starts the coordinator for the provided request over
  // |base_view_controller|'s presentation context.  Does nothing if
  // SetRequest() has not yet been called.
  void StartDialogCoordinator();

  // Stops the coordinator for the provided request.  Does nothing if
  // SetRequest() has not yet been called.
  void StopDialogCoordinator();

  // Returns the view controller showing the JavaScript dialog UI.
  UIViewController* GetAlertViewController();

 private:
  web::WebTaskEnvironment task_environment_;
  ScopedKeyWindow scoped_key_window_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;
  UIViewController* base_view_controller;
  std::unique_ptr<OverlayRequest> request_;
  FakeOverlayRequestCoordinatorDelegate delegate_;
  OverlayRequestCoordinator* coordinator_;
};

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOGS_TEST_JAVA_SCRIPT_DIALOG_OVERLAY_COORDINATOR_TEST_H_
