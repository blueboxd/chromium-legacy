// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_TRANSITIONS_TAB_GRID_TRANSITION_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_TRANSITIONS_TAB_GRID_TRANSITION_HANDLER_H_

#import <UIKit/UIKit.h>

@protocol GridTransitionAnimationLayoutProviding;

// Handler for the transitions between the TabGrid and the Browser.
@interface TabGridTransitionHandler : NSObject

- (instancetype)initWithLayoutProvider:
    (id<GridTransitionAnimationLayoutProviding>)layoutProvider
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Whether the animations should be disabled.
@property(nonatomic, assign) BOOL animationDisabled;

// Starts the transition from the |browser| to the |tabGrid|. Assumes that the
// |browser| is currently a child ViewController of the |tabGrid|. Calls
// |completion| when the transition finishes.
- (void)transitionFromBrowser:(UIViewController*)browser
                    toTabGrid:(UIViewController*)tabGrid
               withCompletion:(void (^)(void))completion;

// Starts the transition from |tabGrid| to |browser|. Adds |browser| as a child
// ViewController of |tabGrid|, covering it. calls |completion| when the
// transition finishes.
- (void)transitionFromTabGrid:(UIViewController*)tabGrid
                    toBrowser:(UIViewController*)browser
               withCompletion:(void (^)(void))completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_TRANSITIONS_TAB_GRID_TRANSITION_HANDLER_H_
