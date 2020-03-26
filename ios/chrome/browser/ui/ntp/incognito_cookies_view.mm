// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/incognito_cookies_view.h"

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/colors/dynamic_color_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kHorizontalSpacing = 14.0f;

const CGFloat kVerticalSpacing = 16.0f;

const CGFloat kVerticalLabelMargin = 6.0f;

}  // namespace

@implementation IncognitoCookiesView

#pragma mark - UIView

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    UIColor* bodyTextColor = color::DarkModeDynamicColor(
        [UIColor colorNamed:kTextSecondaryColor], true,
        [UIColor colorNamed:kTextSecondaryDarkColor]);
    self.layer.borderWidth = 1;
    self.layer.borderColor = bodyTextColor.CGColor;
    self.layer.cornerRadius = 10;

    // Cookies title.
    UILabel* titleLabel = [[UILabel alloc] init];
    titleLabel.textColor = UIColor.whiteColor;
    titleLabel.adjustsFontForContentSizeCategory = YES;
    titleLabel.numberOfLines = 0;
    titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCallout];
    titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    titleLabel.text =
        l10n_util::GetNSString(IDS_NEW_TAB_OTR_THIRD_PARTY_COOKIE);

    [self addSubview:titleLabel];

    // Cookies description.
    UILabel* descriptionLabel = [[UILabel alloc] init];
    descriptionLabel.textColor = bodyTextColor;
    descriptionLabel.adjustsFontForContentSizeCategory = YES;
    descriptionLabel.numberOfLines = 0;
    descriptionLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    descriptionLabel.translatesAutoresizingMaskIntoConstraints = NO;
    descriptionLabel.text =
        l10n_util::GetNSString(IDS_NEW_TAB_OTR_THIRD_PARTY_COOKIE_SUBLABEL);

    [self addSubview:descriptionLabel];

    // Cookies switch.
    _switchView = [[UISwitch alloc] init];
    [_switchView setOn:NO];
    [_switchView addTarget:self
                    action:@selector(onCookieSwitchToggled:)
          forControlEvents:UIControlEventValueChanged];
    _switchView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_switchView];
    [_switchView
        setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh + 1
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];

    NSArray* constraints = @[
      // titleLabel constraints.
      [titleLabel.leadingAnchor constraintEqualToAnchor:self.leadingAnchor
                                               constant:kHorizontalSpacing],
      [titleLabel.trailingAnchor
          constraintEqualToAnchor:_switchView.leadingAnchor
                         constant:-kHorizontalSpacing],
      [titleLabel.trailingAnchor
          constraintEqualToAnchor:descriptionLabel.trailingAnchor],
      [titleLabel.topAnchor constraintEqualToAnchor:self.topAnchor
                                           constant:kVerticalSpacing],

      // descriptionLabel constraints.
      [descriptionLabel.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:kHorizontalSpacing],
      [descriptionLabel.topAnchor
          constraintEqualToAnchor:titleLabel.bottomAnchor
                         constant:kVerticalLabelMargin],
      [descriptionLabel.bottomAnchor constraintEqualToAnchor:self.bottomAnchor
                                                    constant:-kVerticalSpacing],

      // switchView constraints.
      [_switchView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor
                                                 constant:-kHorizontalSpacing],
      [_switchView.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    ];
    [NSLayoutConstraint activateConstraints:constraints];
  }
  return self;
}

#pragma mark - Private

- (void)onCookieSwitchToggled:(UISwitch*)paramSender {
  // TODO(crbug.com/1063824): Implement this.
}
@end
