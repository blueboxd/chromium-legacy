// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/ui/settings/password/reauthentication_module.h"

#import <LocalAuthentication/LocalAuthentication.h>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

constexpr char kPasscodeArticleURL[] = "https://support.apple.com/HT204060";

using password_manager::metrics_util::LogPasswordSettingsReauthResult;
using password_manager::metrics_util::ReauthResult;

@implementation ReauthenticationModule {
  // Block that creates a new |LAContext| object everytime one is required,
  // meant to make testing with a mock object possible.
  LAContext* (^_createLAContext)(void);

  // Accessor allowing the module to request the update of the time when the
  // successful re-authentication was performed and to get the time of the last
  // successful re-authentication.
  __weak id<SuccessfulReauthTimeAccessor> _successfulReauthTimeAccessor;
}

- (instancetype)initWithSuccessfulReauthTimeAccessor:
    (id<SuccessfulReauthTimeAccessor>)successfulReauthTimeAccessor {
  DCHECK(successfulReauthTimeAccessor);
  self = [super init];
  if (self) {
    _createLAContext = ^{
      return [[LAContext alloc] init];
    };
    _successfulReauthTimeAccessor = successfulReauthTimeAccessor;
  }
  return self;
}

- (BOOL)canAttemptReauth {
  LAContext* context = _createLAContext();
  // The authentication method is Touch ID, Face ID or passcode.
  return [context canEvaluatePolicy:LAPolicyDeviceOwnerAuthentication
                              error:nil];
}

- (void)attemptReauthWithLocalizedReason:(NSString*)localizedReason
                    canReusePreviousAuth:(BOOL)canReusePreviousAuth
                                 handler:(void (^)(BOOL success))handler {
  if (canReusePreviousAuth && [self isPreviousAuthValid]) {
    handler(YES);
    LogPasswordSettingsReauthResult(ReauthResult::kSkipped);
    return;
  }

  LAContext* context = _createLAContext();

  __weak ReauthenticationModule* weakSelf = self;
  void (^replyBlock)(BOOL, NSError*) = ^(BOOL success, NSError* error) {
    dispatch_async(dispatch_get_main_queue(), ^{
      ReauthenticationModule* strongSelf = weakSelf;
      if (!strongSelf)
        return;
      if (success) {
        [strongSelf->_successfulReauthTimeAccessor updateSuccessfulReauthTime];
      }
      handler(success);

      LogPasswordSettingsReauthResult(success ? ReauthResult::kSuccess
                                              : ReauthResult::kFailure);
    });
  };

  [context evaluatePolicy:LAPolicyDeviceOwnerAuthentication
          localizedReason:localizedReason
                    reply:replyBlock];
}

- (BOOL)isPreviousAuthValid {
  BOOL previousAuthValid = NO;
  const int kIntervalForValidAuthInSeconds = 60;
  NSDate* lastSuccessfulReauthTime =
      [_successfulReauthTimeAccessor lastSuccessfulReauthTime];
  if (lastSuccessfulReauthTime) {
    NSDate* currentTime = [NSDate date];
    NSTimeInterval timeSincePreviousSuccessfulAuth =
        [currentTime timeIntervalSinceDate:lastSuccessfulReauthTime];
    if (timeSincePreviousSuccessfulAuth < kIntervalForValidAuthInSeconds) {
      previousAuthValid = YES;
    }
  }
  return previousAuthValid;
}

#pragma mark - ForTesting

- (void)setCreateLAContext:(LAContext* (^)(void))createLAContext {
  _createLAContext = createLAContext;
}

@end
