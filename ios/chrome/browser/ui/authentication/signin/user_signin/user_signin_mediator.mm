// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_mediator.h"

#include <memory>

#include "base/logging.h"
#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface UserSigninMediator ()

// Manager for the authentication flow.
@property(nonatomic, strong) AuthenticationFlow* authenticationFlow;
// Chrome interface to the iOS shared authentication library.
@property(nonatomic, assign) AuthenticationService* authenticationService;

@end

@implementation UserSigninMediator

#pragma mark - Public

- (instancetype)initWithAuthenticationService:
    (AuthenticationService*)authenticationService {
  self = [super init];
  if (self) {
    _authenticationService = authenticationService;
  }
  return self;
}

- (void)cancelSignin {
  if (!self.isAuthenticationCompleted) {
    [self.delegate userSigninMediatorSigninFinishedWithResult:
                       SigninCoordinatorResultCanceledByUser];
    self.isAuthenticationCompleted = YES;
  } else if (self.isAuthenticationInProgress) {
    // TODO(crbug.com/971989): Do not remove until the migration has been
    // completed to ensure that the metrics calculated remain the same
    // throughout the code changes.
    base::RecordAction(base::UserMetricsAction("Signin_Undo_Signin"));
    [self.authenticationFlow cancelAndDismiss];
    self.authenticationService->SignOut(signin_metrics::ABORT_SIGNIN,
                                        /*force_clear_browsing_data=*/false,
                                        nil);
    [self.delegate userSigninMediatorNeedPrimaryButtonUpdate];
  }
}

#pragma mark - State

- (BOOL)isAuthenticationInProgress {
  return self.authenticationFlow != nil;
}

@end
