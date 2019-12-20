// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SIGNIN_INTERACTION_SIGNIN_INTERACTION_CONTROLLER_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_SIGNIN_INTERACTION_SIGNIN_INTERACTION_CONTROLLER_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// The app interface for Sign-in interaction controller tests.
@interface SigninInteractionControllerAppInterface : NSObject

// If |shown| is set to NO, activity indicator is not visible while sign-in is
// in progress.
+ (void)setActivityIndicatorShown:(BOOL)shown;

@end

#endif  // IOS_CHROME_BROWSER_UI_SIGNIN_INTERACTION_SIGNIN_INTERACTION_CONTROLLER_APP_INTERFACE_H_
