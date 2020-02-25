// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/account_verification_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation AccountVerificationProvider

- (void)isValidProviderAccountID:(NSString*)accountID
               completionHandler:(void (^)(BOOL, NSError*))completionHandler {
  // Default implementation return always true.
  dispatch_async(dispatch_get_main_queue(), ^{
    completionHandler(YES, nil);
  });
}

@end
