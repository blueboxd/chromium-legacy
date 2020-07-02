// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/scoped_ui_blocker.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ScopedUIBlocker::ScopedUIBlocker(SceneState* scene) : scene_(scene) {
  DCHECK(scene_);
  AppState* appState = scene.appState;
  DCHECK(appState.sceneShowingBlockingUI == nil ||
         appState.sceneShowingBlockingUI == scene_)
      << "Another scene is already showing a blocking UI!";
  [appState incrementBlockingUICounterForScene:scene];
}

ScopedUIBlocker::~ScopedUIBlocker() {
  [scene_.appState decrementBlockingUICounter];
}
