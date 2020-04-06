
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/qr_generator/qr_generator_view_controller.h"

#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kQRGeneratorDoneButtonId = @"kQRGeneratorDoneButtonId";

@implementation QRGeneratorViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  [self setUpNavigation];
}

#pragma mark - Private Methods

- (void)setUpNavigation {
  // Set shadowImage to an empty image to remove the separator between the
  // navigation bar and the main content.
  self.navigationController.navigationBar.shadowImage = [[UIImage alloc] init];

  // Set background color.
  [self.navigationController.navigationBar setTranslucent:NO];
  [self.navigationController.navigationBar
      setBarTintColor:UIColor.cr_systemBackgroundColor];
  [self.navigationController.view
      setBackgroundColor:UIColor.cr_systemBackgroundColor];

  // Add a Done button to the navigation bar.
  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self.handler
                           action:@selector(hideQRCode)];
  doneButton.accessibilityIdentifier = kQRGeneratorDoneButtonId;
  self.navigationItem.rightBarButtonItem = doneButton;
}

@end
