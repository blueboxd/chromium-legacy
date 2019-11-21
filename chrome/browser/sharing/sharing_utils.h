// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_UTILS_H_
#define CHROME_BROWSER_SHARING_SHARING_UTILS_H_

#include <string>

namespace syncer {
class SyncService;
}  // namespace syncer

// Returns true if required sync feature is enabled.
bool IsSyncEnabledForSharing(syncer::SyncService* sync_service);

// Returns true if required sync feature is disabled.
bool IsSyncDisabledForSharing(syncer::SyncService* sync_service);

#endif  // CHROME_BROWSER_SHARING_SHARING_UTILS_H_
