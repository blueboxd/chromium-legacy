// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/overlay_container_coordinator.h"

#include <map>
#include <memory>

#include "base/logging.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/overlays/overlay_container_view_controller.h"
#import "ios/chrome/browser/ui/overlays/overlay_presentation_context_impl.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OverlayContainerCoordinator () <
    OverlayContainerViewControllerDelegate>
// Whether the coordinator is started.
@property(nonatomic, assign, getter=isStarted) BOOL started;
// The UI delegate that is used to drive presentation for this container.
@property(nonatomic, readonly) OverlayPresentationContextImpl* UIDelegate;
@end

@implementation OverlayContainerCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  modality:(OverlayModality)modality {
  if (self = [super initWithBaseViewController:viewController
                                       browser:browser]) {
    OverlayPresentationContextImpl::Container::CreateForUserData(browser,
                                                                 browser);
    _UIDelegate =
        OverlayPresentationContextImpl::Container::FromUserData(browser)
            ->PresentationContextForModality(modality);
    DCHECK(_UIDelegate);
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (self.started)
    return;
  self.started = YES;
  // Create the container view controller and add it to the base view
  // controller.
  OverlayContainerViewController* viewController =
      [[OverlayContainerViewController alloc] init];
  viewController.definesPresentationContext = YES;
  viewController.delegate = self;
  _viewController = viewController;
  UIView* containerView = _viewController.view;
  containerView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.baseViewController addChildViewController:_viewController];
  [self.baseViewController.view addSubview:containerView];
  AddSameConstraints(containerView, self.baseViewController.view);
  [_viewController didMoveToParentViewController:self.baseViewController];
}

- (void)stop {
  if (!self.started)
    return;
  self.started = NO;
  self.UIDelegate->SetCoordinator(nil);
  // Remove the container view and reset the view controller.
  [_viewController willMoveToParentViewController:nil];
  [_viewController.view removeFromSuperview];
  [_viewController removeFromParentViewController];
  _viewController = nil;
}

#pragma mark - OverlayContainerViewControllerDelegate

- (void)containerViewController:
            (OverlayContainerViewController*)containerViewController
                didMoveToWindow:(UIWindow*)window {
  // UIViewController presentation no-ops when attempted on window-less parent
  // view controllers.  Wait to set UI delegate's coordinator until the
  // container is added to a window.
  self.UIDelegate->SetCoordinator(window ? self : nil);
}

@end
