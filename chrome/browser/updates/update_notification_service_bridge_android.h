// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_BRIDGE_ANDROID_H_
#define CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_BRIDGE_ANDROID_H_

#include "chrome/browser/updates/update_notification_service_bridge.h"

namespace updates {

class UpdateNotificationServiceBridgeAndroid
    : public UpdateNotificationServiceBridge {
 public:
  UpdateNotificationServiceBridgeAndroid() = default;
  ~UpdateNotificationServiceBridgeAndroid() override = default;

 private:
  // UpdateNotificationServiceBridge implementation.
  void UpdateLastShownTimeStamp(base::Time timestamp) override;
  base::Optional<base::Time> GetLastShownTimeStamp() override;
  void UpdateThrottleInterval(base::TimeDelta interval) override;
  base::Optional<base::TimeDelta> GetThrottleInterval() override;
  void UpdateNegativeActionCount(int count) override;
  int GetNegativeActionCount() override;
  void LaunchChromeActivity(int state) override;

  DISALLOW_COPY_AND_ASSIGN(UpdateNotificationServiceBridgeAndroid);
};

}  // namespace updates

#endif  // CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_BRIDGE_ANDROID_H_
