// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_SERVER_MAC_SERVICE_DELEGATE_H_
#define CHROME_UPDATER_SERVER_MAC_SERVICE_DELEGATE_H_

#import <Foundation/Foundation.h>

#include "base/memory/scoped_refptr.h"

namespace updater {
class UpdateService;
}

@interface CRUUpdateCheckXPCServiceDelegate : NSObject <NSXPCListenerDelegate>

// Designated initializer.
- (instancetype)initWithUpdateService:
    (scoped_refptr<updater::UpdateService>)service NS_DESIGNATED_INITIALIZER;

@end

#endif  // CHROME_UPDATER_SERVER_MAC_SERVICE_DELEGATE_H_
