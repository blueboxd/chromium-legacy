// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_container/browser_container_coordinator.h"

#include "base/logging.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_view_controller.h"
#import "ios/chrome/browser/ui/overlays/overlay_container_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BrowserContainerCoordinator ()
// Whether the coordinator is started.
@property(nonatomic, assign, getter=isStarted) BOOL started;
// The overlay container coordinator for OverlayModality::kWebContentArea.
@property(nonatomic, strong)
    OverlayContainerCoordinator* webContentAreaOverlayContainerCoordinator;
@end

@implementation BrowserContainerCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  if (self.started)
    return;
  self.started = YES;
  DCHECK(self.browser);
  DCHECK(!_viewController);
  BrowserContainerViewController* viewController =
      [[BrowserContainerViewController alloc] init];
  self.webContentAreaOverlayContainerCoordinator =
      [[OverlayContainerCoordinator alloc]
          initWithBaseViewController:viewController
                             browser:self.browser
                            modality:OverlayModality::kWebContentArea];
  [self.webContentAreaOverlayContainerCoordinator start];
  viewController.webContentsOverlayContainerViewController =
      self.webContentAreaOverlayContainerCoordinator.viewController;
  _viewController = viewController;

  [super start];
}

- (void)stop {
  if (!self.started)
    return;
  self.started = NO;
  [self.webContentAreaOverlayContainerCoordinator stop];
  _viewController = nil;
  [super stop];
}

@end
