// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_BRIDGE_H_
#define CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_BRIDGE_H_

#include "base/optional.h"
#include "base/time/time.h"

namespace updates {

// Functions for calling into UpdateNotifiactionServiceBridge.java.

// Updates and persists |timestamp| in Android shared preference.
void UpdateLastShownTimeStamp(base::Time timestamp);

// Return persisted timestamp of last shown notification from Android shared
// preference. Return nullopt if there is no data.
base::Optional<base::Time> GetLastShownTimestamp();

}  // namespace updates

#endif  // CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_BRIDGE_H_
