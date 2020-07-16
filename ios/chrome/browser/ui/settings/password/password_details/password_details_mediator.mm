// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_mediator.h"

#include "components/autofill/core/common/password_form.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_view_controller_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {
struct PasswordForm;
}

@interface PasswordDetailsMediator () <PasswordDetailsViewControllerDelegate> {
  autofill::PasswordForm _password;
}

@end

@implementation PasswordDetailsMediator

- (instancetype)initWithPassword:(const autofill::PasswordForm&)passwordForm {
  self = [super init];
  if (self) {
    _password = passwordForm;
  }
  return self;
}

- (void)setConsumer:(id<PasswordDetailsConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;

  PasswordDetails* password =
      [[PasswordDetails alloc] initWithPasswordForm:_password];

  [self.consumer setPassword:password];
}

#pragma mark - PasswordDetailsViewControllerDelegate
- (void)passwordDetailsViewController:
            (PasswordDetailsViewController*)viewController
               didEditPasswordDetails:(PasswordDetails*)password {
  // TODO:(crbug.com/1075494) - Edit password accordingly.
}

@end
