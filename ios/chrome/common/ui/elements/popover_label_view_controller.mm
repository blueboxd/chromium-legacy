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
// Distance between the primary text label and the secondary text label.
constexpr CGFloat kVerticalDistance = 24;

}  // namespace

@interface PopoverLabelViewController () <
    UIPopoverPresentationControllerDelegate>

// The main message being presented.
@property(nonatomic, strong, readonly) NSString* message;

// The attributed string being presented as primary text.
@property(nonatomic, strong, readonly)
    NSAttributedString* primaryAttributedString;

// The attributed string being presented as secondary text.
@property(nonatomic, strong, readonly)
    NSAttributedString* secondaryAttributedString;

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

- (instancetype)initWithPrimaryAttributedString:
                    (NSAttributedString*)primaryAttributedString
                      secondaryAttributedString:
                          (NSAttributedString*)secondaryAttributedString {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _primaryAttributedString = primaryAttributedString;
    _secondaryAttributedString = secondaryAttributedString;
    self.modalPresentationStyle = UIModalPresentationPopover;
    self.popoverPresentationController.delegate = self;
  }
  return self;
}

#pragma mark - UIViewController

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

  UILayoutGuide* textLayoutGuide = [[UILayoutGuide alloc] init];
  [self.view addLayoutGuide:textLayoutGuide];
  AddSameConstraints(textLayoutGuide, scrollView);

  UILabel* primaryLabel = [[UILabel alloc] init];
  primaryLabel.numberOfLines = 0;
  primaryLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  primaryLabel.textAlignment = NSTextAlignmentNatural;
  primaryLabel.adjustsFontForContentSizeCategory = YES;
  if (self.message) {
    primaryLabel.text = self.message;
  } else if (self.primaryAttributedString) {
    primaryLabel.attributedText = self.primaryAttributedString;
  }
  primaryLabel.translatesAutoresizingMaskIntoConstraints = NO;
  primaryLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  [scrollView addSubview:primaryLabel];

  UILabel* secondaryLabel = [[UILabel alloc] init];
  secondaryLabel.numberOfLines = 0;
  secondaryLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  secondaryLabel.textAlignment = NSTextAlignmentNatural;
  secondaryLabel.adjustsFontForContentSizeCategory = YES;
  if (self.secondaryAttributedString) {
    secondaryLabel.attributedText = self.secondaryAttributedString;
  }
  secondaryLabel.translatesAutoresizingMaskIntoConstraints = NO;
  secondaryLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  [scrollView addSubview:secondaryLabel];

  NSLayoutConstraint* heightConstraint = [scrollView.heightAnchor
      constraintEqualToAnchor:scrollView.contentLayoutGuide.heightAnchor
                   multiplier:1];

  // UILayoutPriorityDefaultHigh is the default priority for content
  // compression. Setting this lower avoids compressing the content of the
  // scroll view.
  heightConstraint.priority = UILayoutPriorityDefaultHigh - 1;
  heightConstraint.active = YES;

  // Set the compression resistance to high priority to avoid the text
  // being trimmed when using large font size. Set the primary text with higher
  // priority so if the space can't contain both text, the secondary text will
  // be trimmed.
  [primaryLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh + 2
                                      forAxis:UILayoutConstraintAxisVertical];
  [secondaryLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh + 1
                                      forAxis:UILayoutConstraintAxisVertical];

  CGFloat verticalOffset =
      (secondaryLabel.attributedText) ? -kVerticalDistance : 0;
  NSLayoutConstraint* verticalConstraint = [primaryLabel.bottomAnchor
      constraintEqualToAnchor:secondaryLabel.topAnchor
                     constant:verticalOffset];

  [NSLayoutConstraint activateConstraints:@[
    [textLayoutGuide.widthAnchor
        constraintEqualToAnchor:scrollView.widthAnchor],
    [textLayoutGuide.leadingAnchor
        constraintEqualToAnchor:primaryLabel.leadingAnchor],
    [textLayoutGuide.leadingAnchor
        constraintEqualToAnchor:secondaryLabel.leadingAnchor],
    [textLayoutGuide.trailingAnchor
        constraintEqualToAnchor:primaryLabel.trailingAnchor],
    [textLayoutGuide.trailingAnchor
        constraintEqualToAnchor:secondaryLabel.trailingAnchor],
    verticalConstraint,
    [textLayoutGuide.topAnchor constraintEqualToAnchor:primaryLabel.topAnchor
                                              constant:-kInsetValue],
    [textLayoutGuide.bottomAnchor
        constraintEqualToAnchor:secondaryLabel.bottomAnchor
                       constant:kInsetValue],
  ]];
}

- (void)viewWillAppear:(BOOL)animated {
  [self updatePreferredContentSize];
  [super viewWillAppear:animated];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if ((self.traitCollection.verticalSizeClass !=
       previousTraitCollection.verticalSizeClass) ||
      (self.traitCollection.horizontalSizeClass !=
       previousTraitCollection.horizontalSizeClass)) {
    [self updatePreferredContentSize];
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (UIModalPresentationStyle)
    adaptivePresentationStyleForPresentationController:
        (UIPresentationController*)controller
                                       traitCollection:
                                           (UITraitCollection*)traitCollection {
  return UIModalPresentationNone;
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
  self.preferredContentSize = size;
}

@end
