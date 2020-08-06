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
// Leading and trailing margin for label and button to the container border.
// Different margins based on feed being visible or hidden.
const CGFloat kHeaderMarginFeedVisible = 20;
const CGFloat kHeaderMarginFeedHidden = 9;
// Leading and trailing margin for the header container (space between border
// and frame).
const CGFloat kHeaderBorderMargin = 16;
// Font size for label text in header.
const CGFloat kDiscoverFeedTitleFontSize = 16;
// Insets for header menu button.
const CGFloat kHeaderMenuButtonInsetTopAndBottom = 11;
const CGFloat kHeaderMenuButtonInsetSides = 2;
// Duration for the header animation when Discover feed visibility changes.
const CGFloat kHeaderChangeAnimationDuration = 0.5;
// Border properties for the header's 'Off' state.
const CGFloat kHeaderBorderWidth = 1;
const CGFloat kHeaderBorderRadius = 8;
}

#pragma mark - ContentSuggestionsDiscoverHeaderItem

@implementation ContentSuggestionsDiscoverHeaderItem

- (instancetype)initWithType:(NSInteger)type discoverFeedVisible:(BOOL)visible {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsDiscoverHeaderCell class];
    _discoverFeedVisible = visible;
  }
  return self;
}

- (void)configureCell:(ContentSuggestionsDiscoverHeaderCell*)cell {
  [super configureCell:cell];
  cell.titleLabel.text =
      self.discoverFeedVisible
          ? self.title
          : [NSString
                stringWithFormat:@"%@ – %@", self.title,
                                 l10n_util::GetNSString(
                                     IDS_IOS_DISCOVER_FEED_TITLE_OFF_LABEL)];
  [cell changeDiscoverFeedHeaderVisibility:self.discoverFeedVisible];
}

@end

#pragma mark - ContentSuggestionsDiscoverHeaderCell

@interface ContentSuggestionsDiscoverHeaderCell ()

// Represents whether the Discover feed is visible or hidden. NSNumber allows
// for nil value before being set.
@property(nonatomic) NSNumber* discoverFeedVisible;

// Container for the header which allows for adding a border and animation.
@property(nonatomic, strong) UIView* container;

// Header constraints for when the feed is visible.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* feedVisibleConstraints;

// Header constraints for when the feed is hidden.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* feedHiddenConstraints;

@end

@implementation ContentSuggestionsDiscoverHeaderCell

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _container = [[UIView alloc] init];
    _container.translatesAutoresizingMaskIntoConstraints = NO;

    _titleLabel = [[UILabel alloc] init];
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.font = [UIFont systemFontOfSize:kDiscoverFeedTitleFontSize
                                         weight:UIFontWeightMedium];
    _titleLabel.textColor = [UIColor colorNamed:kGrey700Color];
    _titleLabel.adjustsFontForContentSizeCategory = YES;

    _menuButton = [[UIButton alloc] init];
    _menuButton.translatesAutoresizingMaskIntoConstraints = NO;
    _menuButton.accessibilityIdentifier =
        kContentSuggestionsDiscoverHeaderButtonIdentifier;
    [_menuButton
        setImage:[[UIImage imageNamed:@"infobar_settings_icon"]
                     imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]
        forState:UIControlStateNormal];
    _menuButton.tintColor = [UIColor colorNamed:kGrey600Color];
    _menuButton.imageEdgeInsets = UIEdgeInsetsMake(
        kHeaderMenuButtonInsetTopAndBottom, kHeaderMenuButtonInsetSides,
        kHeaderMenuButtonInsetTopAndBottom, kHeaderMenuButtonInsetSides);

    [_container addSubview:_menuButton];
    [_container addSubview:_titleLabel];
    [self.contentView addSubview:_container];

    [NSLayoutConstraint activateConstraints:@[
      [_container.topAnchor constraintEqualToAnchor:self.contentView.topAnchor],
      [_container.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor],

      [_titleLabel.topAnchor constraintEqualToAnchor:_container.topAnchor],
      [_titleLabel.bottomAnchor
          constraintEqualToAnchor:_container.bottomAnchor],
      [_titleLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:_menuButton.leadingAnchor],

      [_menuButton.topAnchor constraintEqualToAnchor:_container.topAnchor],
      [_menuButton.bottomAnchor
          constraintEqualToAnchor:_container.bottomAnchor],
    ]];

    _feedVisibleConstraints = @[
      [_container.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor],
      [_container.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor],
      [_titleLabel.leadingAnchor
          constraintEqualToAnchor:_container.leadingAnchor
                         constant:kHeaderMarginFeedVisible],
      [_menuButton.trailingAnchor
          constraintEqualToAnchor:_container.trailingAnchor
                         constant:-kHeaderMarginFeedVisible],
    ];

    _feedHiddenConstraints = @[
      [_container.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kHeaderBorderMargin],
      [_container.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kHeaderBorderMargin],
      [_titleLabel.leadingAnchor
          constraintEqualToAnchor:_container.leadingAnchor
                         constant:kHeaderMarginFeedHidden],
      [_menuButton.trailingAnchor
          constraintEqualToAnchor:_container.trailingAnchor
                         constant:-kHeaderMarginFeedHidden],
    ];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  [self.menuButton removeTarget:nil
                         action:nil
               forControlEvents:UIControlEventAllEvents];
}

- (void)changeDiscoverFeedHeaderVisibility:(BOOL)visible {
  // Checks is discoverFeedVisible value is nil, indicating that the header has
  // been newly created or reloaded.
  if (self.discoverFeedVisible) {
    if ([self.discoverFeedVisible boolValue] == visible) {
      return;
    }
    // If the header already exists, force the animation by setting other header
    // view first. This is because the header constraints are lost when the NTP
    // is reloaded, which happens when toggling the visibility.
    visible ? [self setHiddenFeedHeader] : [self setVisibleFeedHeader];
    [self.contentView layoutIfNeeded];
    [UIView animateWithDuration:kHeaderChangeAnimationDuration
                     animations:^{
                       visible ? [self setVisibleFeedHeader]
                               : [self setHiddenFeedHeader];
                       [self.contentView layoutIfNeeded];
                     }];
  } else {
    visible ? [self setVisibleFeedHeader] : [self setHiddenFeedHeader];
  }
  self.discoverFeedVisible = [NSNumber numberWithBool:visible];
}

#pragma mark - Private

// Sets header properties for when the Discover feed is visible.
- (void)setVisibleFeedHeader {
  self.container.layer.borderWidth = 0;
  [NSLayoutConstraint deactivateConstraints:self.feedHiddenConstraints];
  [NSLayoutConstraint activateConstraints:self.feedVisibleConstraints];
}

// Sets header properties for when the Discover feed is hidden.
- (void)setHiddenFeedHeader {
  self.container.layer.borderColor = [UIColor colorNamed:kGrey300Color].CGColor;
  self.container.layer.borderWidth = kHeaderBorderWidth;
  self.container.layer.cornerRadius = kHeaderBorderRadius;
  [NSLayoutConstraint deactivateConstraints:self.feedVisibleConstraints];
  [NSLayoutConstraint activateConstraints:self.feedHiddenConstraints];
}

@end
