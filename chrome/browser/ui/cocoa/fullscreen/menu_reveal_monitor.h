// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_FULLSCREEN_MENU_REVEAL_MONITOR_H_
#define CHROME_BROWSER_UI_COCOA_FULLSCREEN_MENU_REVEAL_MONITOR_H_

#import <Cocoa/Cocoa.h>

// MenuRevealMonitor tracks visibility of the menu bar associated with |window|,
// and calls |handler| when it changes. In fullscreen, when the mouse pointer
// moves to or away from the top of the screen, |handler| will be called several
// times with a number between zero and one indicating how much of the menu bar
// is visible.
@interface MenuRevealMonitor : NSObject
- (instancetype)initWithWindow:(NSWindow*)window
                 changeHandler:(void (^)(double))handler
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
@end

#endif  // CHROME_BROWSER_UI_COCOA_FULLSCREEN_MENU_REVEAL_MONITOR_H_
