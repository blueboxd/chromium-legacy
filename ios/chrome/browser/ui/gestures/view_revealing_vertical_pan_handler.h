// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_GESTURES_VIEW_REVEALING_VERTICAL_PAN_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_GESTURES_VIEW_REVEALING_VERTICAL_PAN_HANDLER_H_

#import <UIKit/UIKit.h>
#import "ios/chrome/browser/ui/gestures/view_revealing_animatee.h"

// Responsible for handling vertical pan gestures to reveal/hide a view behind
// another.
@interface ViewRevealingVerticalPanHandler : NSObject

- (instancetype)initWithHeight:(CGFloat)height;
// When creating a pan gesture, this method is passed as the action on the init
// method. It is called when a gesture starts, moves, or ends.
- (void)handlePanGesture:(UIPanGestureRecognizer*)gesture;

// Adds UI element to list of animated objects.
- (void)addAnimatee:(id<ViewRevealingAnimatee>)animatee;

// Height of the view that will be revealed.
@property(nonatomic, assign, readonly) CGFloat viewHeight;

@end

#endif  // IOS_CHROME_BROWSER_UI_THUMB_STRIP_VIEW_REVEALING_VERTICAL_PAN_HANDLER_H_
