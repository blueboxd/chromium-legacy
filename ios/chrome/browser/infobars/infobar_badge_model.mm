// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/infobar_badge_model.h"

#import "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarBadgeModel ()

// The type of Infobar associated with this badge.
@property(nonatomic, assign) InfobarType infobarType;

@end

@implementation InfobarBadgeModel
// Synthesized from protocol.
@synthesize tappable = _tappable;
// Synthesized from protocol.
@synthesize accepted = _accepted;

- (instancetype)initWithInfobarType:(InfobarType)type accepted:(BOOL)accepted {
  self = [super init];
  if (self) {
    _tappable = YES;
    _infobarType = type;
    _accepted = accepted;
  }
  return self;
}

#pragma mark - BadgeViewModel

- (BadgeType)badgeType {
  switch (self.infobarType) {
    case InfobarType::kInfobarTypePasswordSave:
      return BadgeType::kBadgeTypePasswordSave;
    case InfobarType::kInfobarTypePasswordUpdate:
      return BadgeType::kBadgeTypePasswordUpdate;
    default:
      NOTREACHED() << "This infobar should not have a badge";
      return BadgeType::kBadgeTypeNone;
  }
}

@end
