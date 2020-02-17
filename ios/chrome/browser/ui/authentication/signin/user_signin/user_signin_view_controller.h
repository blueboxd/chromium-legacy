// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_SIGNIN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_SIGNIN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// View controller used to show sign-in UI.
@interface UserSigninViewController : UIViewController

// View controller that handles the user consent before the user signs in.
@property UIViewController* unifiedConsentViewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_SIGNIN_VIEW_CONTROLLER_H_
