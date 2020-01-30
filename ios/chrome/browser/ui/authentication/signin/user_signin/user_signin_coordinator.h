// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_SIGNIN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_SIGNIN_COORDINATOR_H_

#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"

// Coordinates the user sign-in with different intents:
//  + user sign-in when triggered from UI (settings, bookmarks...)
//  + first run sign-in
//  + Chrome upgrade sign-in
@interface UserSigninCoordinator : SigninCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
    NS_UNAVAILABLE;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:(ChromeBrowserState*)browserState
    NS_UNAVAILABLE;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Designated initializer.
// |viewController| presents the sign-in.
// |identity| is the identity preselected with the sign-in opens.
// |accessPoint| is the view where the sign-in button was displayed.
// |promoAction| is promo button used to trigger the sign-in.
- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                      identity:(ChromeIdentity*)identity
                   accessPoint:(signin_metrics::AccessPoint)accessPoint
                   promoAction:(signin_metrics::PromoAction)promoAction
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_SIGNIN_COORDINATOR_H_
