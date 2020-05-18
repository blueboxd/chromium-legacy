// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Inset for the text content.
constexpr CGFloat kInsetValue = 20;
// Desired percentage of the width of the presented view controller.
constexpr CGFloat kWidthProportion = 0.75;

}  // namespace

@interface PopoverLabelViewController () <
    UIPopoverPresentationControllerDelegate>

// The message being presented.
@property(nonatomic, strong, readonly) NSString* message;

@end

@implementation PopoverLabelViewController

- (instancetype)initWithMessage:(NSString*)message {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _message = message;
    self.modalPresentationStyle = UIModalPresentationPopover;
    self.popoverPresentationController.delegate = self;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.backgroundColor = UIColor.clearColor;
  scrollView.delaysContentTouches = NO;
  scrollView.showsVerticalScrollIndicator = YES;
  scrollView.showsHorizontalScrollIndicator = NO;
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:scrollView];
  AddSameConstraints(self.view.layoutMarginsGuide, scrollView);

  UILabel* label = [[UILabel alloc] init];
  label.numberOfLines = 0;
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  label.textAlignment = NSTextAlignmentNatural;
  label.adjustsFontForContentSizeCategory = YES;
  label.text = self.message;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  [scrollView addSubview:label];

  AddSameConstraints(label, scrollView);

  [label.widthAnchor constraintEqualToAnchor:scrollView.widthAnchor].active =
      YES;

  NSLayoutConstraint* heightConstraint = [scrollView.heightAnchor
      constraintEqualToAnchor:scrollView.contentLayoutGuide.heightAnchor
                   multiplier:1];
  // UILayoutPriorityDefaultHigh is the default priority for content
  // compression. Setting this lower avoids compressing the content of the
  // scroll view.
  heightConstraint.priority = UILayoutPriorityDefaultHigh - 1;
  heightConstraint.active = YES;
}

- (UIModalPresentationStyle)
    adaptivePresentationStyleForPresentationController:
        (UIPresentationController*)controller
                                       traitCollection:
                                           (UITraitCollection*)traitCollection {
  return UIModalPresentationNone;
}

- (void)updateViewConstraints {
  [self updatePreferredContentSize];
  [super updateViewConstraints];
}

#pragma mark - Helpers

// Updates the preferred content size according to the presenting view size and
// the layout size of the view.
- (void)updatePreferredContentSize {
  CGFloat width =
      self.presentingViewController.view.bounds.size.width * kWidthProportion;

  CGSize size = [self.view systemLayoutSizeFittingSize:CGSizeMake(width, 0)
                         withHorizontalFittingPriority:UILayoutPriorityRequired
                               verticalFittingPriority:500];
  self.preferredContentSize =
      CGSizeMake(size.width, size.height + 2 * kInsetValue);
}

@end
