// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/qr_generator/qr_generator_coordinator.h"

#import "base/mac/foundation_util.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/qr_generation_commands.h"
#import "ios/chrome/browser/ui/qr_generator/qr_generator_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/test/scoped_key_window.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class QRGeneratorCoordinatorTest : public PlatformTest {
 protected:
  QRGeneratorCoordinatorTest() : browser_(std::make_unique<TestBrowser>()) {
    base_view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
  }

  void SetUp() override {
    mock_qr_generation_commands_handler_ =
        OCMStrictProtocolMock(@protocol(QRGenerationCommands));

    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_qr_generation_commands_handler_
                     forProtocol:@protocol(QRGenerationCommands)];
  }

  base::test::TaskEnvironment task_environment_;
  id mock_qr_generation_commands_handler_;
  std::unique_ptr<TestBrowser> browser_;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* base_view_controller_;
};

// Tests that a Done button gets added to the navigation bar, and its action
// dispatches the right command.
TEST_F(QRGeneratorCoordinatorTest, Done_DispatchesCommand) {
  // Set-up mocked handler.
  [[mock_qr_generation_commands_handler_ expect] hideQRCode];

  // Create and start coordinator.
  QRGeneratorCoordinator* coordinator = [[QRGeneratorCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                           title:@"Does not matter"
                             URL:GURL()];

  ASSERT_EQ(base_view_controller_, coordinator.baseViewController);
  ASSERT_FALSE(base_view_controller_.presentedViewController);

  [coordinator start];

  ASSERT_TRUE(base_view_controller_.presentedViewController);
  ASSERT_TRUE([base_view_controller_.presentedViewController
      isKindOfClass:[QRGeneratorViewController class]]);

  QRGeneratorViewController* viewController =
      base::mac::ObjCCastStrict<QRGeneratorViewController>(
          base_view_controller_.presentedViewController);

  // Mimick click on done button.
  [viewController.actionHandler confirmationAlertDone];

  // Callback should've gotten invoked.
  [mock_qr_generation_commands_handler_ verify];
}
