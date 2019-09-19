// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_SCENE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_MAIN_SCENE_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/main/scene_state.h"

// The controller object for a scene. Reacts to scene state changes.
@interface SceneController : NSObject <SceneStateObserver>

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithSceneState:(SceneState*)sceneState
    NS_DESIGNATED_INITIALIZER;

// The state of the scene controlled by this object.
@property(nonatomic, weak, readonly) SceneState* sceneState;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_SCENE_CONTROLLER_H_
