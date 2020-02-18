// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_SERVER_MAC_SERVICE_PROTOCOL_H_
#define CHROME_UPDATER_SERVER_MAC_SERVICE_PROTOCOL_H_

#import <Foundation/Foundation.h>

// Protocol for the XPC update checking service.
@protocol UpdateChecking <NSObject>
// Checks for updates and returns the result in the reply block.
- (void)checkForUpdatesWithReply:(void (^_Nullable)(int rc))reply;
@end

#endif  // CHROME_UPDATER_SERVER_MAC_SERVICE_PROTOCOL_H_
