// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_PUBLIC_CPP_SHARING_WEBRTC_METRICS_H_
#define CHROME_SERVICES_SHARING_PUBLIC_CPP_SHARING_WEBRTC_METRICS_H_

namespace sharing {

// State of the WebRTC connection when it timed out.
// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "SharingWebRtcTimeoutState" in src/tools/metrics/histograms/enums.xml.
enum class WebRtcTimeoutState {
  kConnecting = 0,
  kMessageReceived = 1,
  kMessageSent = 2,
  kDisconnecting = 3,
  kMaxValue = kDisconnecting,
};

// Logs whether adding ice candidate was successful.
void LogWebRtcAddIceCandidate(bool success);

// Logs number of ice servers fetched from network traversal api call.
void LogWebRtcIceConfigFetched(int count);

// Logs that the WebRTC connection timed out while in |state|.
void LogWebRtcTimeout(WebRtcTimeoutState state);

}  // namespace sharing

#endif  // CHROME_SERVICES_SHARING_PUBLIC_CPP_SHARING_WEBRTC_METRICS_H_
