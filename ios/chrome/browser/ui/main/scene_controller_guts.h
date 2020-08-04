// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_SCENE_CONTROLLER_GUTS_H_
#define IOS_CHROME_BROWSER_UI_MAIN_SCENE_CONTROLLER_GUTS_H_

#import <UIKit/UIKit.h>

@protocol SceneControllerGuts

#pragma mark - iOS 12 compat

- (void)startUpChromeUI;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_SCENE_CONTROLLER_GUTS_H_
