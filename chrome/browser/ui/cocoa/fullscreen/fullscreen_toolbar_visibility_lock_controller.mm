// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_visibility_lock_controller.h"

#include "base/mac/scoped_nsobject.h"

#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_animation_controller.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_controller.h"

@interface FullscreenToolbarVisibilityLockController () {
  // Stores the objects that are locking the toolbar visibility.
  base::scoped_nsobject<NSMutableSet> _visibilityLocks;

  // Our owner.
  FullscreenToolbarController* _owner;  // weak

  // The object managing the fullscreen toolbar's animations.
  FullscreenToolbarAnimationController* _animationController;  // weak
}

@end

@implementation FullscreenToolbarVisibilityLockController

- (instancetype)
initWithFullscreenToolbarController:(FullscreenToolbarController*)owner
                animationController:
                    (FullscreenToolbarAnimationController*)animationController {
  if ((self = [super init])) {
    _animationController = animationController;
    _owner = owner;

    // Create the toolbar visibility lock set; 10 is arbitrary, but should
    // hopefully be big enough to hold all locks that'll ever be needed.
    _visibilityLocks.reset([[NSMutableSet setWithCapacity:10] retain]);
  }

  return self;
}

- (BOOL)isToolbarVisibilityLocked {
  return [_visibilityLocks count];
}

- (BOOL)isToolbarVisibilityLockedForOwner:(id)owner {
  return [_visibilityLocks containsObject:owner];
}

- (void)lockToolbarVisibilityForOwner:(id)owner withAnimation:(BOOL)animate {
  if ([self isToolbarVisibilityLockedForOwner:owner])
    return;

  [_visibilityLocks addObject:owner];

  if (animate)
    _animationController->AnimateToolbarIn();
  else
    [_owner layoutToolbar];
}

- (void)releaseToolbarVisibilityForOwner:(id)owner withAnimation:(BOOL)animate {
  if (![self isToolbarVisibilityLockedForOwner:owner])
    return;

  [_visibilityLocks removeObject:owner];

  if (animate)
    _animationController->AnimateToolbarOutIfPossible();
  else
    [_owner layoutToolbar];
}

@end
