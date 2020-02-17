// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/transitions/tab_grid_transition_handler.h"

#import "ios/chrome/browser/ui/tab_grid/transitions/grid_transition_animation.h"
#import "ios/chrome/browser/ui/tab_grid/transitions/grid_transition_animation_layout_providing.h"
#import "ios/chrome/browser/ui/tab_grid/transitions/grid_transition_layout.h"
#import "ios/chrome/browser/ui/util/named_guide.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kBrowserToGridDuration = 0.3;
const CGFloat kGridToBrowserDuration = 0.5;
}  // namespace

@interface TabGridTransitionHandler ()
@property(nonatomic, weak) id<GridTransitionAnimationLayoutProviding>
    layoutProvider;
// Animation object for the transition.
@property(nonatomic, strong) GridTransitionAnimation* animation;
@end

@implementation TabGridTransitionHandler

#pragma mark - Public

- (instancetype)initWithLayoutProvider:
    (id<GridTransitionAnimationLayoutProviding>)layoutProvider {
  self = [super init];
  if (self) {
    _layoutProvider = layoutProvider;
  }
  return self;
}

- (void)transitionFromBrowser:(UIViewController*)browser
                    toTabGrid:(UIViewController*)tabGrid
               withCompletion:(void (^)(void))completion {
  // TODO(crbug.com/1038034): Add support for ReducedMotionAnimator.

  [browser willMoveToParentViewController:nil];

  self.animation = [[GridTransitionAnimation alloc]
      initWithLayout:[self transitionLayoutForTabInViewController:browser]
            duration:kBrowserToGridDuration
           direction:GridAnimationDirectionContracting];

  UIView* animationContainer = [self.layoutProvider animationViewsContainer];
  UIView* bottomViewForAnimations =
      [self.layoutProvider animationViewsContainerBottomView];
  [animationContainer insertSubview:self.animation
                       aboveSubview:bottomViewForAnimations];

  [self.animation.animator addAnimations:^{
    [tabGrid setNeedsStatusBarAppearanceUpdate];
  }];

  [self.animation.animator addCompletion:^(UIViewAnimatingPosition position) {
    [self.animation removeFromSuperview];
    if (position == UIViewAnimatingPositionEnd) {
      [browser.view removeFromSuperview];
      [browser removeFromParentViewController];
    }
    if (completion)
      completion();
  }];

  // TODO(crbug.com/850507): Have the tab view animate itself out alongside
  // this transition instead of just zeroing the alpha here.
  browser.view.alpha = 0;

  // Run the main animation.
  [self.animation.animator startAnimation];
}

- (void)transitionFromTabGrid:(UIViewController*)tabGrid
                    toBrowser:(UIViewController*)browser
               withCompletion:(void (^)(void))completion {
  // TODO(crbug.com/1038034): Add support for ReducedMotionAnimator.

  [tabGrid addChildViewController:browser];

  browser.view.frame = tabGrid.view.bounds;
  [tabGrid.view addSubview:browser.view];

  browser.view.alpha = 0;

  self.animation = [[GridTransitionAnimation alloc]
      initWithLayout:[self transitionLayoutForTabInViewController:browser]
            duration:kGridToBrowserDuration
           direction:GridAnimationDirectionExpanding];

  UIView* animationContainer = [self.layoutProvider animationViewsContainer];
  UIView* bottomViewForAnimations =
      [self.layoutProvider animationViewsContainerBottomView];
  [animationContainer insertSubview:self.animation
                       aboveSubview:bottomViewForAnimations];

  [tabGrid.view addSubview:self.animation.activeCell];

  [self.animation.animator addAnimations:^{
    [tabGrid setNeedsStatusBarAppearanceUpdate];
  }];

  [self.animation.animator addCompletion:^(UIViewAnimatingPosition position) {
    [self.animation.activeCell removeFromSuperview];
    [self.animation removeFromSuperview];
    if (position == UIViewAnimatingPositionEnd) {
      browser.view.alpha = 1;
      [browser didMoveToParentViewController:tabGrid];
    }
    if (completion)
      completion();
  }];

  // Run the main animation.
  [self.animation.animator startAnimation];
}

#pragma mark - Private

// Returns the transition layout based on the |browser|.
- (GridTransitionLayout*)transitionLayoutForTabInViewController:
    (UIViewController*)viewControllerForTab {
  GridTransitionLayout* layout = [self.layoutProvider transitionLayout];

  // Get the fram for the snapshotted content of the active tab.
  // Conceptually the transition is dismissing/presenting a tab (a BVC).
  // However, currently the BVC instances are themselves contanted within a
  // BVCContainer view controller. This means that the
  // |viewControllerForTab.view| is not the BVC's view; rather it's the view of
  // the view controller that contains the BVC. Unfortunatley, the layout guide
  // needed here is attached to the BVC's view, which is the first (and only)
  // subview of the BVCContainerViewController's view.
  // TODO(crbug.com/860234) Clean up this arrangement.
  UIView* tabContentView = viewControllerForTab.view.subviews[0];

  CGRect contentArea = [NamedGuide guideWithName:kContentAreaGuide
                                            view:tabContentView]
                           .layoutFrame;

  [layout.activeItem populateWithSnapshotsFromView:tabContentView
                                        middleRect:contentArea];
  layout.expandedRect = [[self.layoutProvider animationViewsContainer]
      convertRect:tabContentView.frame
         fromView:viewControllerForTab.view];

  return layout;
}

@end
