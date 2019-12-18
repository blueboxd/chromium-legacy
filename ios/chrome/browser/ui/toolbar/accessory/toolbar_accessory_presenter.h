// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_ACCESSORY_TOOLBAR_ACCESSORY_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_ACCESSORY_TOOLBAR_ACCESSORY_PRESENTER_H_

#import <UIKit/UIKit.h>

// Presenter that displays accessories over or next to the toolbar. Note that
// there are different presentations styles for iPhone (Compact Toolbar) vs.
// iPad. This is used by Find in Page.
@interface ToolbarAccessoryPresenter : NSObject

// When presenting views, this presenter will add them into the
// |baseViewController|.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                               isIncognito:(BOOL)isIncognito
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The main presented view.
@property(nonatomic, strong, readonly) UIView* backgroundView;

// Adds the provided |toolbarAccessoryView| as an accessory. Calls the provided
// |completion| after adding the view. There can only be one toolbar view
// presented at a time. If there is a view already presented, this is a no-op.
- (void)addToolbarAccessoryView:(UIView*)toolbarAccessoryView
                       animated:(BOOL)animated
                     completion:(void (^)())completion;

// Hides an already-presented view. This must be done before presenting a
// different view.
- (void)hideToolbarAccessoryViewAnimated:(BOOL)animated
                              completion:(void (^)())completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_ACCESSORY_TOOLBAR_ACCESSORY_PRESENTER_H_
