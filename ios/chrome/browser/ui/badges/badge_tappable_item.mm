// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/badges/badge_tappable_item.h"

#import "ios/chrome/browser/ui/badges/badge_type.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BadgeTappableItem ()

// The BadgeType of this item.
@property(nonatomic, assign) BadgeType badgeType;

@end

@implementation BadgeTappableItem
// Synthesized from protocol.
@synthesize tappable = _tappable;
// Synthesized from protocol.
@synthesize accepted = _accepted;

- (instancetype)initWithBadgeType:(BadgeType)badgeType {
  self = [super init];
  if (self) {
    _badgeType = badgeType;
    _tappable = YES;
    _accepted = NO;
  }
  return self;
}

#pragma mark - BadgeItem

- (BOOL)isFullScreen {
  return NO;
}

@end
