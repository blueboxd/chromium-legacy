// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_controller.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_menubar_tracker.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_animation_controller.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_mouse_tracker.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_visibility_lock_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"

@implementation FullscreenToolbarController

- (id)initWithDelegate:(id<FullscreenToolbarContextDelegate>)delegate {
  if ((self = [super init])) {
    _animationController =
        std::make_unique<FullscreenToolbarAnimationController>(self);
    _visibilityLockController.reset(
        [[FullscreenToolbarVisibilityLockController alloc]
            initWithFullscreenToolbarController:self
                            animationController:_animationController.get()]);
  }

  _delegate = delegate;
  return self;
}

- (void)dealloc {
  DCHECK(!_inFullscreenMode);
  [super dealloc];
}

- (void)enterFullscreenMode {
  DCHECK(!_inFullscreenMode);
  _inFullscreenMode = YES;

  _menubarTracker.reset([[FullscreenMenubarTracker alloc]
      initWithFullscreenToolbarController:self]);
  _mouseTracker.reset([[FullscreenToolbarMouseTracker alloc]
      initWithFullscreenToolbarController:self]);
}

- (void)exitFullscreenMode {
  DCHECK(_inFullscreenMode);
  _inFullscreenMode = NO;

  _animationController->StopAnimationAndTimer();
  [[NSNotificationCenter defaultCenter] removeObserver:self];

  _menubarTracker.reset();
  _mouseTracker.reset();
}

- (void)revealToolbarForWebContents:(content::WebContents*)contents
                       inForeground:(BOOL)inForeground {
  _animationController->AnimateToolbarForTabstripChanges(contents,
                                                         inForeground);
}

- (CGFloat)toolbarFraction {
  // Visibility fractions for the menubar and toolbar.
  constexpr CGFloat kHideFraction = 0.0;
  constexpr CGFloat kShowFraction = 1.0;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    return kHideFraction;

  switch (_toolbarStyle) {
    case FullscreenToolbarStyle::TOOLBAR_PRESENT:
      return kShowFraction;
    case FullscreenToolbarStyle::TOOLBAR_NONE:
      return kHideFraction;
    case FullscreenToolbarStyle::TOOLBAR_HIDDEN:
      if (_animationController->IsAnimationRunning())
        return _animationController->GetToolbarFractionFromProgress();

      if ([self mustShowFullscreenToolbar])
        return kShowFraction;

      return [_menubarTracker menubarFraction];
  }
}

- (FullscreenToolbarStyle)toolbarStyle {
  return _toolbarStyle;
}

- (BOOL)mustShowFullscreenToolbar {
  if (!_inFullscreenMode)
    return NO;

  if (_toolbarStyle == FullscreenToolbarStyle::TOOLBAR_PRESENT)
    return YES;

  if (_toolbarStyle == FullscreenToolbarStyle::TOOLBAR_NONE)
    return NO;

  FullscreenMenubarState menubarState = [_menubarTracker state];
  return menubarState == FullscreenMenubarState::SHOWN ||
         [_visibilityLockController isToolbarVisibilityLocked];
}

- (void)updateToolbarFrame:(NSRect)frame {
  if (_mouseTracker.get())
    [_mouseTracker updateToolbarFrame:frame];
}

- (void)layoutToolbar {
  _animationController->ToolbarDidUpdate();
  [_mouseTracker updateTrackingArea];
}

- (BOOL)isInFullscreen {
  return _inFullscreenMode;
}

- (FullscreenMenubarTracker*)menubarTracker {
  return _menubarTracker.get();
}

- (FullscreenToolbarVisibilityLockController*)visibilityLockController {
  return _visibilityLockController.get();
}

- (void)setToolbarStyle:(FullscreenToolbarStyle)style {
  _toolbarStyle = style;
}

- (id<FullscreenToolbarContextDelegate>)delegate {
  return _delegate;
}

@end

@implementation FullscreenToolbarController (ExposedForTesting)

- (FullscreenToolbarAnimationController*)animationController {
  return _animationController.get();
}

- (void)setMenubarTracker:(FullscreenMenubarTracker*)tracker {
  _menubarTracker.reset([tracker retain]);
}

- (void)setMouseTracker:(FullscreenToolbarMouseTracker*)tracker {
  _mouseTracker.reset([tracker retain]);
}

- (void)setTestFullscreenMode:(BOOL)isInFullscreen {
  _inFullscreenMode = isInFullscreen;
}

@end
