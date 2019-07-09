// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BADGES_BADGE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_BADGES_BADGE_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@protocol BadgeConsumer;
class WebStateList;

// Coordinator that presents badges.
@interface BadgeCoordinator : ChromeCoordinator

// The ViewController that the Coordinator is managing.
// TODO(crbug.com/981925): Replace this property with a getter once
// BadgeCoordinator manages its own view controller.
@property(nonatomic, weak) UIViewController<BadgeConsumer>* viewController;

// The WebStateList for this Coordinator.
@property(nonatomic, assign) WebStateList* webStateList;

@end

#endif  // IOS_CHROME_BROWSER_UI_BADGES_BADGE_COORDINATOR_H_
