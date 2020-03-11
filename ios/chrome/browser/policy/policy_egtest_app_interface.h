// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_POLICY_EGTEST_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_POLICY_POLICY_EGTEST_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

@interface PolicyEGTestAppInterface : NSObject

// Returns a JSON-encoded representation of the value for the given |policyKey|.
// Looks for the policy in the platform policy provider under the CHROME policy
// namespace.
+ (NSString*)valueForPlatformPolicy:(NSString*)policyKey;

@end

#endif  // IOS_CHROME_BROWSER_POLICY_POLICY_EGTEST_APP_INTERFACE_H_
