// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "chrome/updater/server/mac/service_protocol.h"

namespace updater {

NSXPCInterface* GetXpcInterface() {
  NSXPCInterface* updateCheckingInterface =
      [NSXPCInterface interfaceWithProtocol:@protocol(CRUUpdateChecking)];
  NSXPCInterface* updateStateObservingInterface =
      [NSXPCInterface interfaceWithProtocol:@protocol(CRUUpdateStateObserving)];

  [updateCheckingInterface
       setInterface:updateStateObservingInterface
        forSelector:@selector(checkForUpdatesWithUpdateState:reply:)
      argumentIndex:0
            ofReply:NO];

  [updateCheckingInterface
       setInterface:updateStateObservingInterface
        forSelector:@selector(checkForUpdateWithAppID:
                                             priority:updateState:reply:)
      argumentIndex:2
            ofReply:NO];

  return updateCheckingInterface;
}

}  // namespace updater
