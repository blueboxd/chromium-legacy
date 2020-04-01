// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_POLICY_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_POLICY_POLICY_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

@interface PolicyAppInterface : NSObject

// Returns a JSON-encoded representation of the value for the given |policyKey|.
// Looks for the policy in the platform policy provider under the CHROME policy
// namespace.
+ (NSString*)valueForPlatformPolicy:(NSString*)policyKey;

// Sets the |SearchSuggestEnabled| policy to the given value.
// TODO(crbug.com/1024115): This should be replaced with a more generic API that
// can set arbitrarily complex policy data. This suggest-specific API only
// exists to allow us to write an example policy EG2 test.
+ (void)setSuggestPolicyEnabled:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_POLICY_POLICY_APP_INTERFACE_H_
