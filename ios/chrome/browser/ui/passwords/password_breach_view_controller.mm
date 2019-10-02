// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/password_breach_view_controller.h"

#include "base/strings/sys_string_conversions.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::SysUTF16ToNSString;
using password_manager::GetAcceptButtonLabel;
using password_manager::GetCancelButtonLabel;
using password_manager::GetDescription;
using password_manager::GetTitle;
using password_manager::CredentialLeakType;

namespace {
constexpr CGFloat kStackViewSpacing = 16.0;
}  // namespace

@implementation PasswordBreachViewController

#pragma mark - Public

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  // TODO(crbug.com/1008862): Pass these at init instead of using these empty
  // ones.
  CredentialLeakType leakType = CredentialLeakType();
  GURL URL = GURL();

  UIButton* doneButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [doneButton addTarget:self
                 action:@selector(didTapDoneButton)
       forControlEvents:UIControlEventTouchUpInside];
  NSString* doneTitle = SysUTF16ToNSString(GetCancelButtonLabel());
  [doneButton setTitle:doneTitle forState:UIControlStateNormal];
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
  title.text = SysUTF16ToNSString(GetTitle(leakType));
  title.translatesAutoresizingMaskIntoConstraints = NO;

  UILabel* subtitle = [[UILabel alloc] init];
  subtitle.numberOfLines = 0;
  subtitle.textColor = [UIColor colorNamed:kTextSecondaryColor];
  subtitle.text = SysUTF16ToNSString(GetDescription(leakType, URL));
  subtitle.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView* stackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ imageView, title, subtitle ]];
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.alignment = UIStackViewAlignmentFill;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.spacing = kStackViewSpacing;
  [self.view addSubview:stackView];

  UIButton* primaryActionButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [primaryActionButton addTarget:self
                          action:@selector(didTapPrimaryActionButton)
                forControlEvents:UIControlEventTouchUpInside];
  NSString* primaryActionTitle =
      SysUTF16ToNSString(GetAcceptButtonLabel(leakType));
  [primaryActionButton setTitle:primaryActionTitle
                       forState:UIControlStateNormal];
  primaryActionButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:primaryActionButton];

  UILayoutGuide* margins = self.view.layoutMarginsGuide;

  // Primary Action Button constraints.
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

  // Primary Action Button constraints.
  AddSameConstraintsToSides(
      margins, primaryActionButton,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kBottom);
}

#pragma mark - Private

// Handle taps on the done button.
- (void)didTapDoneButton {
  // TODO(crbug.com/1008862): Hook up with a mediator.
}

// Handle taps on the primary action button.
- (void)didTapPrimaryActionButton {
  // TODO(crbug.com/1008862): Hook up with a mediator.
}

@end
