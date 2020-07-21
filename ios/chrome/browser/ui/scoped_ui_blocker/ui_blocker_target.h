// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SCOPED_UI_BLOCKER_UI_BLOCKER_TARGET_H_
#define IOS_CHROME_BROWSER_UI_SCOPED_UI_BLOCKER_UI_BLOCKER_TARGET_H_

#import <Foundation/Foundation.h>

@protocol UIBlockerManager;

// Target to block all UI.
@protocol UIBlockerTarget <NSObject>

// Returns UI blocker manager.
@property(nonatomic, weak, readonly) id<UIBlockerManager> uiBlockerManager;

@end

#endif  // IOS_CHROME_BROWSER_UI_SCOPED_UI_BLOCKER_UI_BLOCKER_TARGET_H_
