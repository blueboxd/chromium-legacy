// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SHELL_TEST_EARL_GREY_SERVICE_MANAGER_APP_INTERFACE_H_
#define IOS_WEB_SHELL_TEST_EARL_GREY_SERVICE_MANAGER_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// Test methods that perform actions on Web Shell using Mojo Service API. These
// methods may alter Web Shell's internal state programmatically, but in both
// cases will properly synchronize the UI for Earl Grey tests.
@interface ServiceManagerTestAppInterface : NSObject

// Asynchronously logs instance group name via "user_id" Mojo service. Logs are
// accessible via +logs method. eDO does not support asynchronous communication,
// but clients can poll +logs to get instance group.
+ (void)logInstanceGroup;

// Returns logs from various Mojo methods.
+ (NSArray*)logs;

// Clears logs from various Mojo methods.
+ (void)clearLogs;

// Sets web usage flag for the current WebState using Mojo API.
+ (void)setWebUsageEnabled:(BOOL)enabled;

@end

#endif  // IOS_WEB_SHELL_TEST_EARL_GREY_SERVICE_MANAGER_APP_INTERFACE_H_
