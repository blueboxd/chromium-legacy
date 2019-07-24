// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/web_push_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace gcm {

void LogSendWebPushMessageResult(SendWebPushMessageResult result) {
  base::UmaHistogramEnumeration("GCM.SendWebPushMessageResult", result);
}

void LogSendWebPushMessagePayloadSize(int size) {
  // Note: The maximum size accepted by FCM is 4096.
  base::UmaHistogramCounts10000("GCM.SendWebPushMessagePayloadSize", size);
}

}  // namespace gcm
