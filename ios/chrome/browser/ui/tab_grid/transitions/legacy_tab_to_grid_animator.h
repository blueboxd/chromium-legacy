// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_TRANSITIONS_LEGACY_TAB_TO_GRID_ANIMATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_TRANSITIONS_LEGACY_TAB_TO_GRID_ANIMATOR_H_

#import <UIKit/UIKit.h>

@protocol GridTransitionAnimationLayoutProviding;

// Animator object for transitioning from a fullscreen view controller (the
// "tab") into a collection view of square-ish items (the "grid").
@interface LegacyTabToGridAnimator
    : NSObject <UIViewControllerAnimatedTransitioning>

// Initialize an animator object with |animationLayoutProvider| to provide
// layout information for the transition.
- (instancetype)initWithAnimationLayoutProvider:
    (id<GridTransitionAnimationLayoutProviding>)animationLayoutProvider;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_TRANSITIONS_LEGACY_TAB_TO_GRID_ANIMATOR_H_
