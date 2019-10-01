// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/scene_controller.h"

#import "base/logging.h"
#import "ios/chrome/app/chrome_overlay_window.h"
#import "ios/chrome/browser/ui/util/multi_window_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SceneController

- (instancetype)initWithSceneState:(SceneState*)sceneState {
  self = [super init];
  if (self) {
    _sceneState = sceneState;
    [_sceneState addObserver:self];
    // The window is necessary very early in the app/scene lifecycle, so it
    // should be created right away.
    if (!self.sceneState.window) {
      DCHECK(!IsMultiwindowSupported())
          << "The window must be created by the scene delegate";
      self.sceneState.window = [[ChromeOverlayWindow alloc]
          initWithFrame:[[UIScreen mainScreen] bounds]];
    }
  }
  return self;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
}

@end
