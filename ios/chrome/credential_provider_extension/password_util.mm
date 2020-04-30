// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/password_util.h"

#import <Security/Security.h>

#import "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* PasswordWithKeychainIdentifier(NSString* identifier) {
  NSDictionary* query = @{
    (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
    (__bridge id)kSecAttrAccount : identifier,
    (__bridge id)kSecReturnData : @YES
  };

  // Get the keychain item containing the password.
  CFDataRef secDataRef;
  OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query,
                                        (CFTypeRef*)&secDataRef);
  NSData* data = (__bridge_transfer NSData*)secDataRef;
  DCHECK(status == errSecSuccess)
      << "Error retrieving password, OSStatus: " << status;
  NSString* password = [[NSString alloc] initWithData:data
                                             encoding:NSUTF8StringEncoding];
  return password;
}
