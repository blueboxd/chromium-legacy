// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_PUBLIC_CPP_SHARING_WEBRTC_METRICS_H_
#define CHROME_SERVICES_SHARING_PUBLIC_CPP_SHARING_WEBRTC_METRICS_H_

#include <string>

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

// Type of routing used to establish a p2p connection.
// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "SharingWebRtcConnectionType" in src/tools/metrics/histograms/enums.xml.
enum class WebRtcConnectionType {
  kUnknown = 0,
  kHost = 1,
  kServerReflexive = 2,
  kPeerReflexive = 3,
  kRelay = 4,
  kInvalid = 5,
  kMaxValue = kInvalid,
};

// Converts string |type| to WebRtcConnectionType. The valid strings for
// |type| are defined in https://tools.ietf.org/html/rfc5245.
// Note that kInvalid does not have a corresponding valid string.
WebRtcConnectionType StringToWebRtcConnectionType(const std::string& type);

// Logs whether adding ice candidate was successful.
void LogWebRtcAddIceCandidate(bool success);

// Logs number of ice servers fetched from network traversal api call.
void LogWebRtcIceConfigFetched(int count);

// Logs that the WebRTC connection timed out while in |state|.
void LogWebRtcTimeout(WebRtcTimeoutState state);

// Logs the type of connection used in webrtc.
void LogWebRtcConnectionType(WebRtcConnectionType type);

}  // namespace sharing

#endif  // CHROME_SERVICES_SHARING_PUBLIC_CPP_SHARING_WEBRTC_METRICS_H_
