// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_SERVICE_BRIDGE_ANDROID_H_
#define CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_SERVICE_BRIDGE_ANDROID_H_

#include "chrome/browser/offline_pages/prefetch/notifications/prefetch_notification_service_bridge.h"

#include "base/macros.h"

namespace offline_pages {
namespace prefetch {

class PrefetchNotificationServiceBridgeAndroid
    : public PrefetchNotificationServiceBridge {
 public:
  PrefetchNotificationServiceBridgeAndroid();
  ~PrefetchNotificationServiceBridgeAndroid() override;

 private:
  // PrefetchNotificationServiceBridge implementation.
  void LaunchDownloadHome() override;

  DISALLOW_COPY_AND_ASSIGN(PrefetchNotificationServiceBridgeAndroid);
};

}  // namespace prefetch
}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_SERVICE_BRIDGE_ANDROID_H_
