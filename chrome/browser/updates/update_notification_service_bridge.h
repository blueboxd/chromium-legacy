// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_BRIDGE_H_
#define CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_BRIDGE_H_

#include "base/optional.h"
#include "base/time/time.h"

namespace updates {

// Functions for calling into UpdateNotifiactionServiceBridge.java.

// Updates and persists |timestamp| in Android SharedPreferences.
void UpdateLastShownTimeStamp(base::Time timestamp);

// Returns persisted timestamp of last shown notification from Android
// SharedPreferences. Return nullopt if there is no data.
base::Optional<base::Time> GetLastShownTimeStamp();

// Updates and persists |interval| in Android SharedPreferences.
void UpdateThrottleInterval(base::TimeDelta interval);

// Returns persisted interval that might be throttled from Android
// SharedPreferences. Return nullopt if there is no data.
base::Optional<base::TimeDelta> GetThrottleInterval();

// Updates and persists |count| in Android SharedPreferences.
void UpdateUserDismissCount(int count);

// Returns persisted count from Android SharedPreferences.
int GetUserDismissCount();

// Launches Chrome activity after user clicked the notification.
void LaunchChromeActivity();

}  // namespace updates

#endif  // CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_BRIDGE_H_
