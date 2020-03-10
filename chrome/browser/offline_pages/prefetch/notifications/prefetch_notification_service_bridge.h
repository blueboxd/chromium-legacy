// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_SERVICE_BRIDGE_H_
#define CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_SERVICE_BRIDGE_H_

#include "base/macros.h"

namespace offline_pages {
namespace prefetch {
// Functions for calling into PrefetchNotifiactionServiceBridge.java.

// Launch offline home in ChromeActivity.
void LaunchDownloadHome();

}  // namespace prefetch
}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_SERVICE_BRIDGE_H_
