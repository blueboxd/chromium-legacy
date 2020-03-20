// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/transitions/legacy_tab_to_grid_animator.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/tab_grid/transitions/grid_transition_animation.h"
#import "ios/chrome/browser/ui/tab_grid/transitions/grid_transition_animation_layout_providing.h"
#import "ios/chrome/browser/ui/tab_grid/transitions/grid_transition_layout.h"
#import "ios/chrome/browser/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/property_animator_group.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LegacyTabToGridAnimator ()
@property(nonatomic, weak) id<GridTransitionAnimationLayoutProviding>
    animationLayoutProvider;
// Animation object for this transition.
@property(nonatomic, strong) GridTransitionAnimation* animation;
// Transition context passed into this object when the animation is started.
@property(nonatomic, weak) id<UIViewControllerContextTransitioning>
    transitionContext;
@end

@implementation LegacyTabToGridAnimator

- (instancetype)initWithAnimationLayoutProvider:
    (id<GridTransitionAnimationLayoutProviding>)animationLayoutProvider {
  if ((self = [super init])) {
    _animationLayoutProvider = animationLayoutProvider;
  }
  return self;
}

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  return 0.3;
}

- (void)animateTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  // Keep a pointer to the transition context for use in animation delegate
  // callbacks.
  self.transitionContext = transitionContext;

  // Get views and view controllers for this transition.
  UIView* containerView = [transitionContext containerView];
  UIViewController* gridViewController = [transitionContext
      viewControllerForKey:UITransitionContextToViewControllerKey];
  UIView* gridView =
      [transitionContext viewForKey:UITransitionContextToViewKey];
  UIView* dismissingView =
      [transitionContext viewForKey:UITransitionContextFromViewKey];

  // Find the rect for the animating tab by getting the content area layout
  // guide.
  // Add the grid view to the container. This isn't just for the transition;
  // this is how the grid view controller's view is added to the view
  // hierarchy.
  [containerView insertSubview:gridView belowSubview:dismissingView];
  gridView.frame =
      [transitionContext finalFrameForViewController:gridViewController];
  // Normally this view will layout before it's displayed, but in order to build
  // the layout for the animation, |gridView|'s layout needs to be current, so
  // force an update here.
  [gridView layoutIfNeeded];

  // Ask the state provider for the views to use when inserting the animation.
  UIView* animationContainer =
      [self.animationLayoutProvider animationViewsContainer];

  // Get the layout of the grid for the transition.
  GridTransitionLayout* layout =
      [self.animationLayoutProvider transitionLayout];

  // Get the initial rect for the snapshotted content of the active tab.
  // Conceptually this transition is dismissing a tab (a BVC). However,
  // currently the BVC instances are themselves contanted within a BVCContainer
  // view controller. This means that the |dismissingView| is not the BVC's
  // view; rather it's the view of the view controller that contains the BVC.
  // Unfortunatley, the layout guide needed here is attached to the BVC's view,
  // which is the first (and only) subview of the BVCContainerViewController's
  // view in most cases. However, crbug.com/1053452 shows that we cannot just
  // blindly use the first subview. The for-loop below ensures that a layout
  // guide is obtained from the first subview that has it.
  // TODO(crbug.com/860234) Clean up this arrangement.
  UIView* viewWithNamedGuides = nil;
  CGRect initialRect;
  for (viewWithNamedGuides in dismissingView.subviews) {
    NamedGuide* namedGuide = [NamedGuide guideWithName:kContentAreaGuide
                                                  view:viewWithNamedGuides];
    if (namedGuide) {
      initialRect = namedGuide.layoutFrame;
      break;
    }
  }
  // Prefer to crash here at the root cause, rather than crashing later where
  // the reason is more ambiguous.
  CHECK_NE(nil, viewWithNamedGuides);
  [layout.activeItem populateWithSnapshotsFromView:viewWithNamedGuides
                                        middleRect:initialRect];

  layout.expandedRect =
      [animationContainer convertRect:viewWithNamedGuides.frame
                             fromView:dismissingView];

  NSTimeInterval duration = [self transitionDuration:transitionContext];
  // Create the animation view and insert it.
  self.animation = [[GridTransitionAnimation alloc]
      initWithLayout:layout
            duration:duration
           direction:GridAnimationDirectionContracting];

  UIView* bottomViewForAnimations =
      [self.animationLayoutProvider animationViewsContainerBottomView];
  [animationContainer insertSubview:self.animation
                       aboveSubview:bottomViewForAnimations];

  [self.animation.animator addCompletion:^(UIViewAnimatingPosition position) {
    BOOL finished = (position == UIViewAnimatingPositionEnd);
    [self gridTransitionAnimationDidFinish:finished];
  }];

  // TODO(crbug.com/850507): Have the tab view animate itself out alongside this
  // transition instead of just zeroing the alpha here.
  dismissingView.alpha = 0;

  // Run the main animation.
  [self.animation.animator startAnimation];
}

- (void)gridTransitionAnimationDidFinish:(BOOL)finished {
  // Clean up the animation
  [self.animation removeFromSuperview];
  // If the transition was cancelled, restore the dismissing view and
  // remove the grid view.
  // If not, remove the dismissing view.
  UIView* gridView =
      [self.transitionContext viewForKey:UITransitionContextToViewKey];
  UIView* dismissingView =
      [self.transitionContext viewForKey:UITransitionContextFromViewKey];
  if (self.transitionContext.transitionWasCancelled) {
    dismissingView.alpha = 1.0;
    [gridView removeFromSuperview];
  } else {
    [dismissingView removeFromSuperview];
  }
  // Mark the transition as completed.
  [self.transitionContext completeTransition:YES];
}

@end
