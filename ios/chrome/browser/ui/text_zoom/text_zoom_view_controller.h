// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TEXT_ZOOM_TEXT_ZOOM_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TEXT_ZOOM_TEXT_ZOOM_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol BrowserCommands;

@interface TextZoomViewController : UIViewController

@property(nonatomic, weak) id<BrowserCommands> commandHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_TEXT_ZOOM_TEXT_ZOOM_VIEW_CONTROLLER_H_
