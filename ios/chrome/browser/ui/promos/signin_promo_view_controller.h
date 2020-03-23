// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PROMOS_SIGNIN_PROMO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PROMOS_SIGNIN_PROMO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/authentication/chrome_signin_view_controller.h"

@protocol ApplicationCommands;
class Browser;

// Class to display a promotion view to encourage the user to sign on, if
// SSO detects that the user has signed in with another application.
//
// Note: On iPhone, this controller supports portrait orientation only. It
// should always be presented in an |OrientationLimitingNavigationController|.
@interface SigninPromoViewController : ChromeSigninViewController

// Designated initializer.  |browser| must not be nil.
- (instancetype)initWithBrowser:(Browser*)browser
                     dispatcher:(id<ApplicationCommands, BrowsingDataCommands>)
                                    dispatcher;

@end

#endif  // IOS_CHROME_BROWSER_UI_PROMOS_SIGNIN_PROMO_VIEW_CONTROLLER_H_
