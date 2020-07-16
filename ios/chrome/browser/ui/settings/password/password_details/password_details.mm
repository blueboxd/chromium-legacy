// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_ui_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PasswordDetails

- (instancetype)initWithPasswordForm:(const autofill::PasswordForm&)form {
  self = [super init];
  if (self) {
    auto nameWithLink = password_manager::GetShownOriginAndLinkUrl(form);
    _origin = base::SysUTF8ToNSString(nameWithLink.first);
    _website = base::SysUTF8ToNSString(nameWithLink.second.spec());
    _username = base::SysUTF16ToNSString(form.username_value);
    _password = base::SysUTF16ToNSString(form.password_value);
  }
  return self;
}

@end
