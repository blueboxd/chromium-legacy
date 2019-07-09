// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BADGES_BADGE_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_BADGES_BADGE_CONSUMER_H_

@protocol BadgeItem;

// Consumer for the BadgeMediator
@protocol BadgeConsumer <NSObject>
// Notifies the consumer to reset with |badges|.
- (void)setupWithBadges:(NSArray*)badges;
// Notifies the consumer to add a badge with configurations matching
// |badgeItem|.
- (void)addBadge:(id<BadgeItem>)badgeItem;
// Notifies the consumer to remove |badgeItem|.
- (void)removeBadge:(id<BadgeItem>)badgeItem;
// Notifies the consumer to update a badge with new configurations in
// |badgeItem|. If |badgeItem| does not exist, then consumer does nothing.
- (void)updateBadge:(id<BadgeItem>)badgeItem;

@end

#endif  // IOS_CHROME_BROWSER_UI_BADGES_BADGE_CONSUMER_H_
