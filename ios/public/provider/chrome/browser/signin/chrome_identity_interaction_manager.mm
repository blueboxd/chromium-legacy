// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/signin/chrome_identity_interaction_manager.h"

#include <ostream>

#include "base/notreached.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ChromeIdentityInteractionManager
@synthesize delegate = _delegate;

- (BOOL)isCanceling {
  return NO;
}

- (void)addAccountWithCompletion:(SigninCompletionCallback)completion {
  NOTREACHED() << "Subclasses must override this";
}

- (void)reauthenticateUserWithID:(NSString*)userID
                           email:(NSString*)userEmail
                      completion:(SigninCompletionCallback)completion {
  NOTREACHED() << "Subclasses must override this";
}

- (void)cancelAndDismissAnimated:(BOOL)animated {
  NOTREACHED() << "Subclasses must override this";
}

- (void)addAccountWithPresentingViewController:(UIViewController*)viewController
                                    completion:
                                        (SigninCompletionCallback)completion {
  NOTREACHED() << "Subclasses must override this";
}

- (void)addAccountWithPresentingViewController:(UIViewController*)viewController
                                     userEmail:(NSString*)userEmail
                                    completion:
                                        (SigninCompletionCallback)completion {
  NOTREACHED() << "Subclasses must override this";
}

- (void)cancelAddAccountWithAnimation:(BOOL)animated
                           completion:(void (^)(void))completion {
  NOTREACHED() << "Subclasses must override this";
}

@end
