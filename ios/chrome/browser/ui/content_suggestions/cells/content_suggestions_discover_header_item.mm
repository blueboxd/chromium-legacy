// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_discover_header_item.h"

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Leading and trailing margin for label and button.
// TODO(crbug.com/1085419): Get margins from Mulder to align elements with
// cards.
const CGFloat kHeaderMargin = 25;
}

#pragma mark - ContentSuggestionsDiscoverHeaderItem

@interface ContentSuggestionsDiscoverHeaderItem ()

// The title for the feed header label.
@property(nonatomic, copy) NSString* title;

@end

@implementation ContentSuggestionsDiscoverHeaderItem

- (instancetype)initWithType:(NSInteger)type title:(NSString*)title {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsDiscoverHeaderCell class];
    _title = title;
  }
  return self;
}

- (void)configureCell:(ContentSuggestionsDiscoverHeaderCell*)cell {
  [super configureCell:cell];
  cell.titleLabel.text = [self.title uppercaseString];
}

@end

#pragma mark - ContentSuggestionsDiscoverHeaderCell

@implementation ContentSuggestionsDiscoverHeaderCell

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _titleLabel.textColor = UIColor.cr_secondaryLabelColor;
    _titleLabel.adjustsFontForContentSizeCategory = YES;

    _menuButton = [[UIButton alloc] init];
    _menuButton.translatesAutoresizingMaskIntoConstraints = NO;
    _menuButton.accessibilityIdentifier =
        kContentSuggestionsDiscoverHeaderButtonIdentifier;
    [_menuButton setImage:[UIImage imageNamed:@"infobar_settings_icon"]
                 forState:UIControlStateNormal];

    [self.contentView addSubview:_menuButton];
    [self.contentView addSubview:_titleLabel];

    [NSLayoutConstraint activateConstraints:@[
      [_titleLabel.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor],
      [_titleLabel.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor],
      [_titleLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:_menuButton.leadingAnchor],
      [_titleLabel.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kHeaderMargin],

      [_menuButton.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor],
      [_menuButton.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor],
      [_menuButton.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kHeaderMargin],
    ]];
  }
  return self;
}

@end
