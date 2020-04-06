// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/qr_generator/qr_generator_view_controller.h"

#import "ios/chrome/browser/ui/commands/qr_generation_commands.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class QRGeneratorViewControllerTest : public PlatformTest {
 public:
  QRGeneratorViewControllerTest()
      : view_controller_([[QRGeneratorViewController alloc] init]) {}

 protected:
  QRGeneratorViewController* view_controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(QRGeneratorViewControllerTest);
};

// Tests the navigation controller's background is properly set-up.
TEST_F(QRGeneratorViewControllerTest, NavigationController_Background) {
  UIColor* backgroundColor = UIColor.cr_systemBackgroundColor;
  id mockNavController = OCMStrictClassMock([UINavigationController class]);

  id mockNavBar = OCMStrictClassMock([UINavigationBar class]);
  OCMStub([mockNavController navigationBar]).andReturn(mockNavBar);
  [[mockNavBar expect] setBarTintColor:backgroundColor];
  [[mockNavBar expect] setTranslucent:NO];
  [[mockNavBar expect] setShadowImage:[[UIImage alloc] init]];

  id mockNavView = OCMStrictClassMock([UIView class]);
  OCMStub([mockNavController view]).andReturn(mockNavView);
  [[mockNavView expect] setBackgroundColor:backgroundColor];

  id vcPartialMock = OCMPartialMock(view_controller_);
  OCMStub([vcPartialMock navigationController]).andReturn(mockNavController);

  [view_controller_ viewDidLoad];

  [mockNavView verify];
  [mockNavBar verify];
}

// Tests that a Done button gets added to the navigation bar, and its action
// dispatches the right command.
TEST_F(QRGeneratorViewControllerTest, DoneButton_DispatchesCommand) {
  // Set-up mocked handler.
  id mockedHandler = OCMStrictProtocolMock(@protocol(QRGenerationCommands));
  [[mockedHandler expect] hideQRCode];
  [view_controller_ setHandler:mockedHandler];

  // Set-up mocked navigation item.
  __block UIBarButtonItem* capturedButton;
  id navItemMock = OCMStrictClassMock([UINavigationItem class]);
  [[navItemMock expect]
      setRightBarButtonItem:[OCMArg
                                checkWithBlock:^BOOL(UIBarButtonItem* button) {
                                  capturedButton = button;
                                  return !!capturedButton;
                                }]];

  id vcPartialMock = OCMPartialMock(view_controller_);
  OCMStub([vcPartialMock navigationItem]).andReturn(navItemMock);

  [view_controller_ viewDidLoad];

  // Button should've been given to the navigationItem.
  [navItemMock verify];

  // Simulate a click on the button.
  [[UIApplication sharedApplication] sendAction:capturedButton.action
                                             to:capturedButton.target
                                           from:nil
                                       forEvent:nil];

  // Callback should've gotten invoked.
  [mockedHandler verify];
}
