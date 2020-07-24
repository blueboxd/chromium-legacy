// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_menubar_tracker.h"

#include <QuartzCore/QuartzCore.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/stl_util.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_controller.h"
#import "chrome/browser/ui/cocoa/fullscreen/menu_reveal_monitor.h"
#include "ui/base/cocoa/appkit_utils.h"

@interface FullscreenMenubarTracker () {
  FullscreenToolbarController* _controller;        // weak

  base::scoped_nsobject<MenuRevealMonitor> menu_reveal_monitor_;
}

// Returns YES if the mouse is on the same screen as the window.
- (BOOL)isMouseOnScreen;

@end

@implementation FullscreenMenubarTracker

@synthesize state = _state;
@synthesize menubarFraction = _menubarFraction;

- (instancetype)initWithFullscreenToolbarController:
    (FullscreenToolbarController*)controller {
  if ((self = [super init])) {
    _controller = controller;
    _state = FullscreenMenubarState::HIDDEN;

    __block FullscreenMenubarTracker* nonCaptureSelf = self;
    menu_reveal_monitor_.reset([[MenuRevealMonitor alloc]
        initWithWindow:[controller window]
         changeHandler:^(double reveal_amount) {
           [nonCaptureSelf setMenubarProgress:reveal_amount];
         }]);

    // Register for Active Space change notifications.
    [[[NSWorkspace sharedWorkspace] notificationCenter]
        addObserver:self
           selector:@selector(activeSpaceDidChange:)
               name:NSWorkspaceActiveSpaceDidChangeNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self];
  menu_reveal_monitor_.reset();
  [super dealloc];
}

- (CGFloat)menubarFraction {
  return _menubarFraction;
}

- (void)setMenubarProgress:(CGFloat)progress {
  if (![_controller isInAnyFullscreenMode] ||
      [_controller isFullscreenTransitionInProgress]) {
    return;
  }

  // If the menubarFraction increases, check if we are in the right screen
  // so that the toolbar is not revealed on the wrong screen.
  if (![self isMouseOnScreen] && progress > _menubarFraction)
    return;

  // Ignore the menubarFraction changes if the Space is inactive.
  if (![[_controller window] isOnActiveSpace])
    return;

  if (ui::IsCGFloatEqual(progress, 1.0))
    _state = FullscreenMenubarState::SHOWN;
  else if (ui::IsCGFloatEqual(progress, 0.0))
    _state = FullscreenMenubarState::HIDDEN;
  else if (progress < _menubarFraction)
    _state = FullscreenMenubarState::HIDING;
  else if (progress > _menubarFraction)
    _state = FullscreenMenubarState::SHOWING;

  _menubarFraction = progress;
  [_controller layoutToolbar];
  // AppKit drives the menu bar animation from a nested run loop. Flush
  // explicitly so that Chrome's UI updates during the animation.
  [CATransaction flush];
}

- (BOOL)isMouseOnScreen {
  return NSMouseInRect([NSEvent mouseLocation],
                       [[_controller window] screen].frame, false);
}

- (void)activeSpaceDidChange:(NSNotification*)notification {
  _menubarFraction = 0.0;
  _state = FullscreenMenubarState::HIDDEN;
  [_controller layoutToolbar];
}

@end
