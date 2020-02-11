// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_TRANSITIONS_LEGACY_GRID_TO_VISIBLE_TAB_ANIMATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_TRANSITIONS_LEGACY_GRID_TO_VISIBLE_TAB_ANIMATOR_H_

#import <UIKit/UIKit.h>

@protocol GridTransitionAnimationLayoutProviding;

// Animator object for transitioning from a collection view of square-ish items
// (the "grid") into a fullscreen view controller (the "tab").
@interface LegacyGridToVisibleTabAnimator
    : NSObject <UIViewControllerAnimatedTransitioning>

// Initialize an animator object with |animationLayoutProvider| to provide
// layout information for the transition.
- (instancetype)initWithAnimationLayoutProvider:
    (id<GridTransitionAnimationLayoutProviding>)animationLayoutProvider;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_TRANSITIONS_LEGACY_GRID_TO_VISIBLE_TAB_ANIMATOR_H_
