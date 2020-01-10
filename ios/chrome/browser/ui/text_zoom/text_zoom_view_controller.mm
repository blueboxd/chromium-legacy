// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/text_zoom/text_zoom_view_controller.h"

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Horizontal padding between all elements (except the previous/next buttons).
const CGFloat kPadding = 8;
const CGFloat kButtonFontSize = 17;
}

@interface TextZoomViewController ()

@property(nonatomic, strong) UIButton* closeButton;

@end

@implementation TextZoomViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.translatesAutoresizingMaskIntoConstraints = NO;

  [self.view addSubview:self.closeButton];

  [NSLayoutConstraint activateConstraints:@[
    // Close Button.
    [self.closeButton.centerYAnchor
        constraintEqualToAnchor:self.view.centerYAnchor],
    //  [self.closeButton.heightAnchor constraintEqualToConstant:kButtonLength],
    // Use button intrinsic width.
    [self.closeButton.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
                       constant:-kPadding],
  ]];

  [self.closeButton
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh + 1
                                      forAxis:UILayoutConstraintAxisHorizontal];
}

#pragma mark - Private methods (control actions)

- (void)closeButtonWasTapped:(id)sender {
  [self.commandHandler hideTextZoom];
}

#pragma mark - Private property Accessors

// Creates and returns the close button.
- (UIButton*)closeButton {
  if (!_closeButton) {
    _closeButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [_closeButton setTitle:l10n_util::GetNSString(IDS_DONE)
                  forState:UIControlStateNormal];
    _closeButton.translatesAutoresizingMaskIntoConstraints = NO;
    //  _closeButton.accessibilityIdentifier = kFindInPageCloseButtonId;
    _closeButton.titleLabel.font = [UIFont systemFontOfSize:kButtonFontSize];
    [_closeButton addTarget:self
                     action:@selector(closeButtonWasTapped:)
           forControlEvents:UIControlEventTouchUpInside];
  }
  return _closeButton;
}

@end
