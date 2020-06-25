// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_password_check_cell.h"

#include "base/check.h"
#include "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Padding used between the icon and the text labels.
const CGFloat kIconLeadingPadding = 12;

// Size of the icon image.
const CGFloat kIconImageSize = 28;

}  // namespace

@interface SettingsPasswordCheckCell ()

// The image view for the trailing icon.
@property(nonatomic, strong) UIImageView* imageView;

// UIActivityIndicatorView spinning while check is running.
@property(nonatomic, readonly, strong)
    UIActivityIndicatorView* activityIndicator;

// Constraint that is used to show only text without |imageView| or
// |activityIndicator|.
@property(nonatomic, strong) NSLayoutConstraint* onlyTextShownConstraint;

// Constraint that is used to show either |imageView| or |activityIndicator|.
@property(nonatomic, strong)
    NSLayoutConstraint* iconOrIndicatorVisibleConstraint;

@end

@implementation SettingsPasswordCheckCell

@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;
@synthesize activityIndicator = _activityIndicator;
@synthesize imageView = _imageView;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    UIView* contentView = self.contentView;

    _imageView = [[UIImageView alloc] init];
    _imageView.translatesAutoresizingMaskIntoConstraints = NO;
    _imageView.tintColor = UIColor.cr_labelColor;
    _imageView.hidden = YES;
    [contentView addSubview:_imageView];

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.textColor = UIColor.cr_labelColor;
    [contentView addSubview:_textLabel];

    _detailTextLabel = [[UILabel alloc] init];
    _detailTextLabel.numberOfLines = 0;
    _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _detailTextLabel.font =
        [UIFont preferredFontForTextStyle:kTableViewSublabelFontStyle];
    _detailTextLabel.adjustsFontForContentSizeCategory = YES;
    _detailTextLabel.textColor = UIColor.cr_secondaryLabelColor;
    [contentView addSubview:_detailTextLabel];

    _activityIndicator = [[UIActivityIndicatorView alloc] init];
    _activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
    _activityIndicator.hidden = YES;
    [contentView addSubview:_activityIndicator];

    // Constraints.
    UILayoutGuide* textLayoutGuide = [[UILayoutGuide alloc] init];
    [self.contentView addLayoutGuide:textLayoutGuide];

    _onlyTextShownConstraint = [textLayoutGuide.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor
                       constant:-kTableViewHorizontalSpacing];

    _iconOrIndicatorVisibleConstraint = [textLayoutGuide.trailingAnchor
        constraintEqualToAnchor:_imageView.leadingAnchor
                       constant:-kIconLeadingPadding];

    NSLayoutConstraint* heightConstraint = [self.contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kChromeTableViewCellHeight];

    // Set up the constraints assuming that the icon image and activity
    // indicator are hidden.
    [NSLayoutConstraint activateConstraints:@[
      heightConstraint,
      _onlyTextShownConstraint,

      [_imageView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [_imageView.widthAnchor constraintEqualToConstant:kIconImageSize],
      [_imageView.heightAnchor constraintEqualToConstant:kIconImageSize],
      [_imageView.centerYAnchor
          constraintEqualToAnchor:textLayoutGuide.centerYAnchor],
      [_imageView.leadingAnchor
          constraintEqualToAnchor:_activityIndicator.leadingAnchor],

      [_activityIndicator.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [_activityIndicator.widthAnchor constraintEqualToConstant:kIconImageSize],
      [_activityIndicator.heightAnchor
          constraintEqualToConstant:kIconImageSize],
      [_activityIndicator.centerYAnchor
          constraintEqualToAnchor:textLayoutGuide.centerYAnchor],

      [textLayoutGuide.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [textLayoutGuide.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],

      [textLayoutGuide.leadingAnchor
          constraintEqualToAnchor:_textLabel.leadingAnchor],
      [textLayoutGuide.leadingAnchor
          constraintEqualToAnchor:_detailTextLabel.leadingAnchor],
      [textLayoutGuide.trailingAnchor
          constraintEqualToAnchor:_textLabel.trailingAnchor],
      [textLayoutGuide.trailingAnchor
          constraintEqualToAnchor:_detailTextLabel.trailingAnchor],
      [textLayoutGuide.topAnchor constraintEqualToAnchor:_textLabel.topAnchor],
      [textLayoutGuide.bottomAnchor
          constraintEqualToAnchor:_detailTextLabel.bottomAnchor],
      [_textLabel.bottomAnchor
          constraintEqualToAnchor:_detailTextLabel.topAnchor],
    ]];
    // Make sure there are top and bottom margins of at least |margin|.
    AddOptionalVerticalPadding(self.contentView, textLayoutGuide,
                               kTableViewTwoLabelsCellVerticalSpacing);
  }
  return self;
}

- (void)showActivityIndicator {
  if (!self.activityIndicator.hidden)
    return;

  self.imageView.hidden = YES;
  self.activityIndicator.hidden = NO;
  [self.activityIndicator startAnimating];
  [self updateTextConstraints];
}

- (void)hideActivityIndicator {
  if (self.activityIndicator.hidden)
    return;

  [self.activityIndicator stopAnimating];
  self.activityIndicator.hidden = YES;
  [self updateTextConstraints];
}

- (void)setIconImage:(UIImage*)image withTintColor:(UIColor*)color {
  if (color) {
    self.imageView.tintColor = color;
  }
  BOOL hidden = !image;
  self.imageView.image = image;
  if (hidden == self.imageView.hidden)
    return;
  self.imageView.hidden = hidden;
  if (!hidden) {
    [self hideActivityIndicator];
  }
  [self updateTextConstraints];
}

#pragma mark - Private Methods

- (void)updateTextConstraints {
  // Active proper |textLayoutGuide| trailing constraint to show |imageView| or
  // |activityIndicator|.
  if (self.activityIndicator.hidden && self.imageView.hidden) {
    _iconOrIndicatorVisibleConstraint.active = NO;
    _onlyTextShownConstraint.active = YES;
  } else {
    _onlyTextShownConstraint.active = NO;
    _iconOrIndicatorVisibleConstraint.active = YES;
  }
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];

  self.textLabel.text = nil;
  self.detailTextLabel.text = nil;
  self.accessibilityTraits = UIAccessibilityTraitNone;
  [self setIconImage:nil withTintColor:nil];
  [self hideActivityIndicator];
}

@end
