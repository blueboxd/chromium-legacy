// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/password_breach_view_controller.h"

#import "ios/chrome/browser/ui/passwords/password_breach_action_handler.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using l10n_util::GetNSString;

namespace {
constexpr CGFloat kStackViewSpacing = 16.0;
}  // namespace

@interface PasswordBreachViewController ()

// Properties backing up the respective consumer setter.
@property(nonatomic, strong) NSString* titleString;
@property(nonatomic, strong) NSString* subtitleString;
@property(nonatomic, strong) NSString* primaryActionString;
@property(nonatomic) BOOL primaryActionAvailable;

@end

@implementation PasswordBreachViewController

#pragma mark - Public

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  UIButton* doneButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [doneButton addTarget:self
                 action:@selector(didTapDoneButton)
       forControlEvents:UIControlEventTouchUpInside];
  [doneButton setTitle:GetNSString(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)
              forState:UIControlStateNormal];
  doneButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:doneButton];

  UIImage* image = [UIImage imageNamed:@"password_breach_illustration"];
  UIImageView* imageView = [[UIImageView alloc] initWithImage:image];
  imageView.contentMode = UIViewContentModeScaleAspectFit;
  imageView.translatesAutoresizingMaskIntoConstraints = NO;

  UILabel* title = [[UILabel alloc] init];
  title.numberOfLines = 0;
  title.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  title.textColor = [UIColor colorNamed:kTextPrimaryColor];
  title.text = self.titleString;
  title.translatesAutoresizingMaskIntoConstraints = NO;

  UILabel* subtitle = [[UILabel alloc] init];
  subtitle.numberOfLines = 0;
  subtitle.textColor = [UIColor colorNamed:kTextSecondaryColor];
  subtitle.text = self.subtitleString;
  subtitle.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView* stackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ imageView, title, subtitle ]];
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.alignment = UIStackViewAlignmentFill;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.spacing = kStackViewSpacing;
  [self.view addSubview:stackView];

  UILayoutGuide* margins = self.view.layoutMarginsGuide;

  if (self.primaryActionAvailable) {
    UIButton* primaryActionButton =
        [UIButton buttonWithType:UIButtonTypeSystem];
    [primaryActionButton addTarget:self
                            action:@selector(didTapPrimaryActionButton)
                  forControlEvents:UIControlEventTouchUpInside];
    [primaryActionButton setTitle:self.primaryActionString
                         forState:UIControlStateNormal];
    primaryActionButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:primaryActionButton];

    // Primary Action Button constraints.
    AddSameConstraintsToSides(
        margins, primaryActionButton,
        LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kBottom);
  }

  // Done Button constraints.
  AddSameConstraintsToSides(margins, doneButton,
                            LayoutSides::kTrailing | LayoutSides::kTop);

  // Stack View (and its contents) constraints.
  CGFloat imageAspectRatio = image.size.width / image.size.height;
  [imageView.widthAnchor constraintEqualToAnchor:imageView.heightAnchor
                                      multiplier:imageAspectRatio]
      .active = YES;
  AddSameCenterConstraints(margins, stackView);
  AddSameConstraintsToSides(margins, stackView,
                            LayoutSides::kLeading | LayoutSides::kTrailing);
}

#pragma mark - PasswordBreachConsumer

- (void)setTitleString:(NSString*)titleString
            subtitleString:(NSString*)subtitleString
       primaryActionString:(NSString*)primaryActionString
    primaryActionAvailable:(BOOL)primaryActionAvailable {
  self.titleString = titleString;
  self.subtitleString = subtitleString;
  self.primaryActionString = primaryActionString;
  self.primaryActionAvailable = primaryActionAvailable;
}

#pragma mark - Private

// Handle taps on the done button.
- (void)didTapDoneButton {
  [self.actionHandler passwordBreachDone];
}

// Handle taps on the primary action button.
- (void)didTapPrimaryActionButton {
  [self.actionHandler passwordBreachPrimaryAction];
}

@end
