// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_LOGGING_USER_SIGNIN_LOGGER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_LOGGING_USER_SIGNIN_LOGGER_H_

#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

// Logs metrics for user sign-in operations.
@interface UserSigninLogger : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithAccessPoint:(signin_metrics::AccessPoint)accessPoint
                        promoAction:(signin_metrics::PromoAction)promoAction
    NS_DESIGNATED_INITIALIZER;

// View where the sign-in button was displayed.
@property(nonatomic, assign, readonly) signin_metrics::AccessPoint accessPoint;

// Promo button used to trigger the sign-in.
@property(nonatomic, assign, readonly) signin_metrics::PromoAction promoAction;

// Logs sign-in started when the user consent screen is first displayed.
- (void)logSigninStarted;

// Logs sign-in completed when the user has attempted sign-in and obtained a
// result.
- (void)logSigninCompletedWithResult:(SigninCoordinatorResult)signinResult
                        addedAccount:(BOOL)addedAccount
               advancedSettingsShown:(BOOL)advancedSettingsShown;

// Logs sign-in cancellation when sign-in is in progress.
// TODO(crbug.com/971989): Used temporarily to log when sign-in is interrupted,
// |logSigninCompletedWithResult| should be used following the architecture
// migration.
- (void)logUndoSignin;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_LOGGING_USER_SIGNIN_LOGGER_H_
