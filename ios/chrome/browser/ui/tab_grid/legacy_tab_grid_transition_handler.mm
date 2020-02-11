// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/legacy_tab_grid_transition_handler.h"

#import "ios/chrome/browser/ui/tab_grid/transitions/grid_transition_animation_layout_providing.h"
#import "ios/chrome/browser/ui/tab_grid/transitions/legacy_grid_to_visible_tab_animator.h"
#import "ios/chrome/browser/ui/tab_grid/transitions/legacy_tab_to_grid_animator.h"
#import "ios/chrome/browser/ui/tab_grid/transitions/reduced_motion_animator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation LegacyTabGridTransitionHandler

#pragma mark - UIViewControllerTransitioningDelegate

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForPresentedController:(UIViewController*)presented
                         presentingController:(UIViewController*)presenting
                             sourceController:(UIViewController*)source {
  if (!UIAccessibilityIsReduceMotionEnabled() &&
      self.provider.selectedCellVisible) {
    return [[LegacyGridToVisibleTabAnimator alloc]
        initWithAnimationLayoutProvider:self.provider];
  }
  ReducedMotionAnimator* simpleAnimator = [[ReducedMotionAnimator alloc] init];
  simpleAnimator.presenting = YES;
  return simpleAnimator;
}

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForDismissedController:(UIViewController*)dismissed {
  if (!UIAccessibilityIsReduceMotionEnabled()) {
    return [[LegacyTabToGridAnimator alloc]
        initWithAnimationLayoutProvider:self.provider];
  }
  ReducedMotionAnimator* simpleAnimator = [[ReducedMotionAnimator alloc] init];
  simpleAnimator.presenting = NO;
  return simpleAnimator;
}

@end
