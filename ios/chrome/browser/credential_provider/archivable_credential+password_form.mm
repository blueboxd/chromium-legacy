// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/archivable_credential+password_form.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using base::SysUTF8ToNSString;
using base::SysUTF16ToNSString;
using base::UTF8ToUTF16;

}  // namespace

@implementation ArchivableCredential (PasswordForm)

// Returns the equivalent of a unique record identifier. Built from the unique
// columns in the logins database.
NSString* recordIdentifierForPasswordForm(const autofill::PasswordForm& form) {
  // These are the UNIQUE keys in the login database.
  return SysUTF16ToNSString(
      UTF8ToUTF16(form.origin.spec() + "|") + form.username_element +
      UTF8ToUTF16("|") + form.username_value + UTF8ToUTF16("|") +
      form.password_element + UTF8ToUTF16("|" + form.signon_realm));
}

- (instancetype)initWithPasswordForm:(const autofill::PasswordForm&)passwordForm
                             favicon:(NSString*)favicon
                validationIdentifier:(NSString*)validationIdentifier {
  if (passwordForm.origin.is_empty() || passwordForm.blacklisted_by_user ||
      password_manager::IsValidAndroidFacetURI(passwordForm.signon_realm)) {
    return nil;
  }
  std::string site_name = password_manager::GetShownOrigin(passwordForm.origin);
  NSString* keychainIdentifier =
      SysUTF8ToNSString(passwordForm.encrypted_password);
  return [self initWithFavicon:favicon
            keychainIdentifier:keychainIdentifier
                          rank:passwordForm.times_used
              recordIdentifier:recordIdentifierForPasswordForm(passwordForm)
             serviceIdentifier:SysUTF8ToNSString(passwordForm.origin.spec())
                   serviceName:SysUTF8ToNSString(site_name)
                          user:SysUTF16ToNSString(passwordForm.username_value)
          validationIdentifier:validationIdentifier];
}

@end
