// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/reauthentication_handler.h"

#import "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ReauthenticationHandler {
  // Module containing the reauthentication mechanism used accessing passwords.
  __weak id<ReauthenticationProtocol> _weakReauthenticationModule;
}

- (instancetype)initWithReauthenticationModule:
    (id<ReauthenticationProtocol>)reauthenticationModule {
  DCHECK(reauthenticationModule);
  self = [super init];
  if (self) {
    _weakReauthenticationModule = reauthenticationModule;
  }
  return self;
}

- (void)verifyUserWithCompletionHandler:
            (void (^)(ReauthenticationResult))completionHandler
        presentReminderOnViewController:(UIViewController*)viewController {
  DCHECK(viewController.extensionContext);
  if ([_weakReauthenticationModule canAttemptReauth]) {
    [_weakReauthenticationModule
        attemptReauthWithLocalizedReason:
            NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_SCREENLOCK_REASON",
                              @"Access Passwords...")
                    canReusePreviousAuth:YES
                                 handler:completionHandler];
  } else {
    [self showSetPasscodeDialogOnViewController:viewController
                              completionHandler:completionHandler];
  }
}

- (void)showSetPasscodeDialogOnViewController:(UIViewController*)viewController
                            completionHandler:(void (^)(ReauthenticationResult))
                                                  completionHandler {
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:
          NSLocalizedString(
              @"IDS_IOS_CREDENTIAL_PROVIDER_SET_UP_SCREENLOCK_TITLE",
              @"Set A Passcode")
                       message:NSLocalizedString(
                                   @"IDS_IOS_CREDENTIAL_PROVIDER_SET_UP_"
                                   @"SCREENLOCK_CONTENT",
                                   @"To use passwords, you must first set a "
                                   @"passcode on your device.")
                preferredStyle:UIAlertControllerStyleAlert];

  UIAlertAction* learnAction = [UIAlertAction
      actionWithTitle:
          NSLocalizedString(
              @"IDS_IOS_CREDENTIAL_PROVIDER_SET_UP_SCREENLOCK_LEARN_HOW",
              @"Learn How")
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction*) {
                UIResponder* responder = viewController;
                while (responder) {
                  if ([responder respondsToSelector:@selector(openURL:)]) {
                    [responder
                        performSelector:@selector(openURL:)
                             withObject:
                                 [NSURL URLWithString:base::SysUTF8ToNSString(
                                                          kPasscodeArticleURL)]
                             afterDelay:0];
                    break;
                  }
                  responder = responder.nextResponder;
                }
                completionHandler(ReauthenticationResult::kFailure);
              }];
  [alertController addAction:learnAction];
  UIAlertAction* okAction = [UIAlertAction
      actionWithTitle:NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_OK",
                                        @"OK")
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction*) {
                completionHandler(ReauthenticationResult::kFailure);
              }];
  [alertController addAction:okAction];
  alertController.preferredAction = okAction;
  [viewController presentViewController:alertController
                               animated:YES
                             completion:nil];
}

@end
