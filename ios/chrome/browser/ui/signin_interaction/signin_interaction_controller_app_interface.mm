// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/signin_interaction/signin_interaction_controller_app_interface.h"

#include "ios/chrome/browser/ui/authentication/chrome_signin_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SigninInteractionControllerAppInterface

+ (void)setActivityIndicatorShown:(BOOL)shown {
  [ChromeSigninViewController setActivityIndicatorShownForTesting:shown];
}

@end
