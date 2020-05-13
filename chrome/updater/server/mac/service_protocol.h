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

// Checks for the version of the Updater. Returns the result in the reply block.
- (void)getUpdaterVersionWithReply:
    (void (^_Nonnull)(NSString* _Nonnull version))reply;

// Checks for updates and returns the result in the reply block.
- (void)checkForUpdatesWithUpdateState:
            (CRUUpdateStateObserver* _Nonnull)updateState
                                 reply:(void (^_Nonnull)(int rc))reply;

// Checks for update of a given app, with specified priority. Sends repeated
// updates of progress and returns the result in the reply block.
- (void)checkForUpdateWithAppID:(NSString* _Nonnull)appID
                       priority:(CRUPriorityWrapper* _Nonnull)priority
                    updateState:(CRUUpdateStateObserver* _Nonnull)updateState
                          reply:(void (^_Nonnull)(int rc))reply;

// Registers app and returns the result in the reply block.
- (void)registerForUpdatesWithAppId:(NSString* _Nullable)appId
                          brandCode:(NSString* _Nullable)brandCode
                                tag:(NSString* _Nullable)tag
                            version:(NSString* _Nullable)version
               existenceCheckerPath:(NSString* _Nullable)existenceCheckerPath
                              reply:(void (^_Nonnull)(int rc))reply;

// Checks if |version| is newer. Returns the result in the reply block.
- (void)haltForUpdateToVersion:(NSString* _Nonnull)version
                         reply:(void (^_Nonnull)(bool shouldUpdate))reply;

@end

// Protocol for the XPC administration tasks of the Updater.
@protocol CRUAdministering <NSObject>

// Performs the admin task (activate service, uninstall service, or no opp) that
// is relevant to the state of the Updater.
- (void)performAdminTasks;

@end

namespace updater {

// Constructs an NSXPCInterface for a connection using CRUUpdateChecking and
// CRUUpdateStateObserving protocols.
NSXPCInterface* _Nonnull GetXPCUpdateCheckingInterface();

// Constructs an NSXPCInterface for a connection using CRUAdministering
// protocol.
NSXPCInterface* _Nonnull GetXPCAdministeringInterface();

}  // namespace updater

#endif  // CHROME_UPDATER_SERVER_MAC_SERVICE_PROTOCOL_H_
