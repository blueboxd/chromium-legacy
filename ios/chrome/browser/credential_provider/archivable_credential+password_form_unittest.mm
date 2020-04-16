// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/archivable_credential+password_form.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using autofill::PasswordForm;
using ArchivableCredentialPasswordFormTest = PlatformTest;

// Tests the creation of a credential from a password form.
TEST_F(ArchivableCredentialPasswordFormTest, Creation) {
  NSString* username = @"username_value";
  NSString* favicon = @"favicon_value";
  NSString* keychainIdentifier = @"keychain_identifier_value";
  NSString* validationIdentifier = @"validation_identifier_value";
  NSString* url = @"http://www.alpha.example.com/path/and?args=8";

  PasswordForm passwordForm;
  passwordForm.times_used = 10;
  passwordForm.username_element = base::UTF8ToUTF16("username_element");
  passwordForm.password_element = base::UTF8ToUTF16("password_element");
  passwordForm.username_value = base::SysNSStringToUTF16(username);
  passwordForm.encrypted_password = base::SysNSStringToUTF8(keychainIdentifier);
  passwordForm.origin = GURL(base::SysNSStringToUTF16(url));
  ArchivableCredential* credential =
      [[ArchivableCredential alloc] initWithPasswordForm:passwordForm
                                                 favicon:favicon
                                    validationIdentifier:validationIdentifier];

  EXPECT_TRUE(credential);
  EXPECT_EQ(passwordForm.times_used, credential.rank);
  EXPECT_NSEQ(username, credential.user);
  EXPECT_NSEQ(favicon, credential.favicon);
  EXPECT_NSEQ(validationIdentifier, credential.validationIdentifier);
  EXPECT_NSEQ(keychainIdentifier, credential.keychainIdentifier);
  EXPECT_NSEQ(@"alpha.example.com", credential.serviceName);
  EXPECT_NSEQ(@"http://www.alpha.example.com/path/and?args=8|"
              @"username_element|username_value|password_element|",
              credential.recordIdentifier);
}

}  // namespace
