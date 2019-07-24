// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_WEB_PUSH_METRICS_H_
#define COMPONENTS_GCM_DRIVER_WEB_PUSH_METRICS_H_

namespace gcm {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SendWebPushMessageResult {
  kSuccessful = 0,
  kEncryptionFailed = 1,
  kCreateJWTFailed = 2,
  kNetworkError = 3,
  kServerError = 4,
  kParseResponseFailed = 5,
  kMaxValue = kParseResponseFailed,
};

// Logs the |result| to UMA. This should be called when after a web push message
// is sent.
void LogSendWebPushMessageResult(SendWebPushMessageResult result);

// Logs the size of message payload to UMA. This should be called right before a
// web push message is sent.
void LogSendWebPushMessagePayloadSize(int size);

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_WEB_PUSH_METRICS_H_
