// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_SCENE_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_MAIN_SCENE_DELEGATE_H_

#import <UIKit/UIKit.h>

// An object acting as a scene delegate for UIKit. Updates the window state.
@interface SceneDelegate : NSObject <UIWindowSceneDelegate>

@property(nonatomic, strong) UIWindow* window;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_SCENE_DELEGATE_H_
