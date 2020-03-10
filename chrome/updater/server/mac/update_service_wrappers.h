// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_SERVER_MAC_UPDATE_SERVICE_WRAPPERS_H_
#define CHROME_UPDATER_SERVER_MAC_UPDATE_SERVICE_WRAPPERS_H_

#import <Foundation/Foundation.h>

#import "chrome/updater/server/mac/service_protocol.h"
#include "chrome/updater/update_service.h"

using StateChangeCallback =
    base::RepeatingCallback<void(updater::UpdateService::UpdateState)>;

@interface CRUUpdateStateObserver : NSObject <CRUUpdateStateObserving>

@property(readonly, nonatomic) StateChangeCallback callback;

- (instancetype)initWithRepeatingCallback:(StateChangeCallback)callback;

@end

@interface CRUUpdateStateWrapper : NSObject <NSSecureCoding>

@property(readonly, nonatomic) updater::UpdateService::UpdateState updateState;

- (instancetype)initWithUpdateState:
    (updater::UpdateService::UpdateState)updateState;

@end

@interface CRUPriorityWrapper : NSObject <NSSecureCoding>

@property(readonly, nonatomic) updater::UpdateService::Priority priority;

- (instancetype)initWithPriority:(updater::UpdateService::Priority)priority;

@end

#endif  // CHROME_UPDATER_SERVER_MAC_UPDATE_SERVICE_WRAPPERS_H_
