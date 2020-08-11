// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_discover_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The minimum height for the Feed Cell.
const CGFloat kMinimumFeedCellHeight = 50;
}

#pragma mark - ContentSuggestionsDiscoverItem

@interface ContentSuggestionsDiscoverItem ()

// Contains a reference to the last configured cell containing the feed.
@property(nonatomic, weak) ContentSuggestionsDiscoverCell* lastConfiguredCell;

@end

@implementation ContentSuggestionsDiscoverItem
@synthesize suggestionIdentifier;
@synthesize metricsRecorded;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsDiscoverCell class];
  }
  return self;
}

- (void)configureCell:(ContentSuggestionsDiscoverCell*)cell {
  [super configureCell:cell];
  [cell setDiscoverFeedView:self.discoverFeed];
  self.lastConfiguredCell = cell;
}

- (CGFloat)cellHeightForWidth:(CGFloat)width {
  return self.lastConfiguredCell ? [self.lastConfiguredCell feedHeight]
                                 : kMinimumFeedCellHeight;
}

@end

#pragma mark - ContentSuggestionsDiscoverCell

@interface ContentSuggestionsDiscoverCell ()

// The Discover feed which acts as the cell's content.
@property(nonatomic, weak) UIViewController* discoverFeed;

@end

@implementation ContentSuggestionsDiscoverCell

- (void)setDiscoverFeedView:(UIViewController*)discoverFeed {
  if (_discoverFeed == discoverFeed) {
    return;
  }
  _discoverFeed = discoverFeed;
  if (discoverFeed) {
    UIView* discoverView = discoverFeed.view;
    discoverView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:discoverView];
    [NSLayoutConstraint activateConstraints:@[
      [discoverView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor],
      [discoverView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor],
      [discoverView.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor],
      [discoverView.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor],
    ]];
  }
}

- (CGFloat)feedHeight {
  UICollectionView* feedView;
  if (!self.discoverFeed) {
    return kMinimumFeedCellHeight;
  }
  // TODO(crbug.com/1092900): Make this more robust. Will require
  // the provider to expose the feed as a UICollectionViewController.
  for (UIView* view in self.discoverFeed.view.subviews) {
    if ([view isKindOfClass:[UICollectionView class]]) {
      feedView = static_cast<UICollectionView*>(view);
    }
  }
  // In order for the Feed loading spinner to be displayed, the minimum feed
  // height must be kMinimumFeedCellHeight.
  return fmax(feedView.contentSize.height, kMinimumFeedCellHeight);
}

@end
