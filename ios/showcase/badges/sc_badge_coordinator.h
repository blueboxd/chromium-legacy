// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHOWCASE_BADGES_SC_BADGE_COORDINATOR_H_
#define IOS_SHOWCASE_BADGES_SC_BADGE_COORDINATOR_H_

#import "ios/showcase/common/navigation_coordinator.h"

// A11y identifier for button that will replace the displayed badge with the
// overflow badge button.
extern NSString* const kSCShowOverflowDisplayedBadgeButton;

// A11y identifier for button that will show an accepted displayed button.
extern NSString* const kSCShowAcceptedDisplayedBadgeButton;

@interface SCBadgeCoordinator : NSObject <NavigationCoordinator>

@end

#endif  // IOS_SHOWCASE_BADGES_SC_BADGE_COORDINATOR_H_
