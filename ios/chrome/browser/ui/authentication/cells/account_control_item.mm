// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/cells/account_control_item.h"

#include "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation AccountControlItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [AccountControlCell class];
    self.accessibilityTraits |= UIAccessibilityTraitButton;
  }
  return self;
}

#pragma mark - TableViewItem

- (void)configureCell:(AccountControlCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.imageView.image = self.image;
  cell.imageView.tintColor = UIColor.cr_labelColor;

  cell.textLabel.text = self.text;
  cell.textLabel.textColor = UIColor.cr_labelColor;

  cell.detailTextLabel.text = self.detailText;
  cell.detailTextLabel.textColor =
      self.shouldDisplayError ? [UIColor colorNamed:kDestructiveTintColor]
                              : UIColor.cr_secondaryLabelColor;
}

#pragma mark - Helper methods

- (NSAttributedString*)attributedStringForText:(NSString*)text
                                          font:(UIFont*)font
                                         color:(UIColor*)color {
  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  paragraphStyle.lineHeightMultiple = 1.15;
  return [[NSAttributedString alloc]
      initWithString:text
          attributes:@{
            NSParagraphStyleAttributeName : paragraphStyle,
            NSFontAttributeName : font,
            NSForegroundColorAttributeName : color
          }];
}

@end

@interface AccountControlCell () {
  // Constraint used to set padding between image and text when image exists.
  NSLayoutConstraint* _textLeadingAnchorConstraint;
}
@end

@implementation AccountControlCell

@synthesize imageView = _imageView;
@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;
    [self addSubviews];
    [self setViewConstraints];
  }
  return self;
}

// Create and add subviews.
- (void)addSubviews {
  UIView* contentView = self.contentView;
  contentView.clipsToBounds = YES;

  _imageView = [[UIImageView alloc] init];
  _imageView.translatesAutoresizingMaskIntoConstraints = NO;
  [_imageView
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh + 1
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [contentView addSubview:_imageView];

  _textLabel = [[UILabel alloc] init];
  _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  _textLabel.adjustsFontForContentSizeCategory = YES;
  _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_textLabel];

  _detailTextLabel = [[UILabel alloc] init];
  _detailTextLabel.font =
      [UIFont preferredFontForTextStyle:kTableViewSublabelFontStyle];
  _detailTextLabel.adjustsFontForContentSizeCategory = YES;
  _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _detailTextLabel.numberOfLines = 0;
  [contentView addSubview:_detailTextLabel];
}

// Set constraints on subviews.
- (void)setViewConstraints {
  UIView* contentView = self.contentView;

  _textLeadingAnchorConstraint = [_textLabel.leadingAnchor
      constraintEqualToAnchor:_imageView.trailingAnchor];

  [NSLayoutConstraint activateConstraints:@[
    // Set leading anchors.
    [_imageView.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor
                       constant:kTableViewHorizontalSpacing],
    [_detailTextLabel.leadingAnchor
        constraintEqualToAnchor:_textLabel.leadingAnchor],
    _textLeadingAnchorConstraint,

    // Set vertical anchors.
    [_textLabel.topAnchor
        constraintEqualToAnchor:contentView.topAnchor
                       constant:kTableViewLargeVerticalSpacing],
    [_textLabel.bottomAnchor
        constraintEqualToAnchor:_detailTextLabel.topAnchor
                       constant:-kTableViewVerticalSpacing],
    [_imageView.centerYAnchor constraintEqualToAnchor:_textLabel.centerYAnchor],
    [_detailTextLabel.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor
                       constant:-kTableViewLargeVerticalSpacing],

    // Set trailing anchors.
    [_textLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:contentView.trailingAnchor
                                 constant:-kTableViewHorizontalSpacing],
    [_detailTextLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:contentView.trailingAnchor
                                 constant:-kTableViewHorizontalSpacing],
  ]];
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];

  // Adjust the text label preferredMaxLayoutWidth when the parent's width
  // changes, for instance on screen rotation.
  if (_imageView.image) {
    _textLeadingAnchorConstraint.constant = kTableViewHorizontalSpacing;
  } else {
    _textLeadingAnchorConstraint.constant = 0;
  }
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.imageView.image = nil;
  self.textLabel.text = nil;
  self.detailTextLabel.text = nil;
  self.detailTextLabel.textColor = UIColor.cr_secondaryLabelColor;
}

#pragma mark - NSObject(Accessibility)

- (NSString*)accessibilityLabel {
  return [NSString stringWithFormat:@"%@, %@", self.textLabel.text,
                                    self.detailTextLabel.text];
}

@end
