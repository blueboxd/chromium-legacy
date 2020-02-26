// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_SIGNIN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_SIGNIN_MEDIATOR_H_

#import <Foundation/Foundation.h>
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_enums.h"

@class AuthenticationFlow;
class AuthenticationService;

// Delegate that handles interactions with unified consent coordinator.
@protocol UserSigninMediatorDelegate

// Updates sign-in state for the UserSigninCoordinator following sign-in
// finishing its workflow.
- (void)userSigninMediatorSigninFinishedWithResult:
    (SigninCoordinatorResult)signinResult;

// Updates the primary button for the user sign-in screen.
- (void)userSigninMediatorNeedPrimaryButtonUpdate;

@end

// Mediator that handles the sign-in operation.
@interface UserSigninMediator : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithAuthenticationService:
    (AuthenticationService*)authenticationService NS_DESIGNATED_INITIALIZER;

// The delegate.
@property(nonatomic, weak) id<UserSigninMediatorDelegate> delegate;

// Property denoting whether the authentication operation is complete.
@property(nonatomic, assign) BOOL isAuthenticationCompleted;

// Reverts the sign-in operation.
- (void)cancelSignin;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_SIGNIN_MEDIATOR_H_
