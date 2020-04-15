
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/qr_generator/qr_generator_view_controller.h"

#import "ios/chrome/browser/ui/qr_generator/qr_generator_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kQRGeneratorDoneButtonId = @"kQRGeneratorDoneButtonId";

// Height and width of the QR code image, in points.
const CGFloat kQRCodeImageSize = 200.0;

@interface QRGeneratorViewController ()

// View for the QR code image.
@property(nonatomic, strong) UIImageView* qrCodeView;

// Orientation-specific constraints.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* compactHeightConstraints;
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* regularHeightConstraints;

@end

@implementation QRGeneratorViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  [self setUpNavigation];

  self.qrCodeView = [self createQRCodeImageView];

  [self.view addSubview:self.qrCodeView];

  [self setupConstraints];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  if (previousTraitCollection.verticalSizeClass !=
      self.traitCollection.verticalSizeClass) {
    [self updateViewConstraints];
  }
}

- (void)updateViewConstraints {
  // Check if we're in landscape mode or not.
  if (IsCompactHeight(self)) {
    [NSLayoutConstraint deactivateConstraints:self.regularHeightConstraints];
    [NSLayoutConstraint activateConstraints:self.compactHeightConstraints];
  } else {
    [NSLayoutConstraint deactivateConstraints:self.compactHeightConstraints];
    [NSLayoutConstraint activateConstraints:self.regularHeightConstraints];
  }
  [super updateViewConstraints];
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

- (UIImageView*)createQRCodeImageView {
  NSData* urlData =
      [[self.pageURL absoluteString] dataUsingEncoding:NSUTF8StringEncoding];
  UIImage* qrCodeImage = GenerateQRCode(urlData, kQRCodeImageSize);

  UIImageView* qrCodeView = [[UIImageView alloc] initWithImage:qrCodeImage];
  qrCodeView.translatesAutoresizingMaskIntoConstraints = NO;
  return qrCodeView;
}

- (void)setupConstraints {
  // Set-up orientation-specific constraints.
  self.compactHeightConstraints = @[
    [self.qrCodeView.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                              constant:20.0],
  ];
  self.regularHeightConstraints = @[
    [self.qrCodeView.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                              constant:60.0],
  ];

  // Activate the right orientation-specific constraint.
  [self updateViewConstraints];

  // Activate common constraints.
  NSArray* commonConstraints = @[
    [self.qrCodeView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
  ];
  [NSLayoutConstraint activateConstraints:commonConstraints];
}

@end
