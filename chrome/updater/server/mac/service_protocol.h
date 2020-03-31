// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_SERVER_MAC_SERVICE_PROTOCOL_H_
#define CHROME_UPDATER_SERVER_MAC_SERVICE_PROTOCOL_H_

#import <Foundation/Foundation.h>

#include "chrome/updater/registration_data.h"
#include "chrome/updater/update_service.h"

@class CRUUpdateStateObserver;
@class CRUUpdateStateWrapper;
@class CRUPriorityWrapper;

// Protocol which observes the state of the XPC update checking service.
@protocol CRUUpdateStateObserving <NSObject>

// Checks for updates and returns the result in the reply block.
- (void)observeUpdateState:(CRUUpdateStateWrapper* _Nonnull)updateState;

@end

// Protocol for the XPC update checking service.
@protocol CRUUpdateChecking <NSObject>

// Checks for updates and returns the result in the reply block.
- (void)checkForUpdatesWithUpdateState:
            (CRUUpdateStateObserver* _Nonnull)updateState
                                 reply:(void (^_Nullable)(int rc))reply;

// Checks for update of a given app, with specified priority. Sends repeated
// updates of progress and returns the result in the reply block.
- (void)checkForUpdateWithAppID:(NSString* _Nonnull)appID
                       priority:(CRUPriorityWrapper* _Nonnull)priority
                    updateState:(CRUUpdateStateObserver* _Nonnull)updateState
                          reply:(void (^_Nullable)(int rc))reply;

// Registers app and returns the result in the reply block.
- (void)registerForUpdatesWithAppId:(NSString* _Nullable)appId
                          brandCode:(NSString* _Nullable)brandCode
                                tag:(NSString* _Nullable)tag
                            version:(NSString* _Nullable)version
               existenceCheckerPath:(NSString* _Nullable)existenceCheckerPath
                              reply:(void (^_Nullable)(int rc))reply;

@end

namespace updater {

// Constructs an NSXPCInterface for a connection using CRUUpdateChecking and
// CRUUpdateStateObserving protocols.
NSXPCInterface* _Nonnull GetXpcInterface();

}  // namespace updater

#endif  // CHROME_UPDATER_SERVER_MAC_SERVICE_PROTOCOL_H_
