// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_PUBLIC_CPP_SHARING_WEBRTC_METRICS_H_
#define CHROME_SERVICES_SHARING_PUBLIC_CPP_SHARING_WEBRTC_METRICS_H_

#include <string>

#include "base/optional.h"
#include "base/time/time.h"

namespace sharing {

// State of the WebRTC connection when it timed out on the browser process.
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

// Result of sending a SharingMessage via WebRTC.
// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "SharingWebRtcSendMessageResult" in src/tools/metrics/histograms/enums.xml.
enum class WebRtcSendMessageResult {
  kInternalError = 0,
  kSuccess = 1,
  kEmptyMessage = 2,
  kPayloadTooLarge = 3,
  kBufferExceeded = 4,
  kConnectionClosed = 5,
  kDataChannelNotReady = 6,
  kMaxValue = kDataChannelNotReady,
};

// Error reason for closing a p2p WebRTC connection.
// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "SharingWebRtcConnectionErrorReason" in
// src/tools/metrics/histograms/enums.xml.
enum class WebRtcConnectionErrorReason {
  kInvalidRemoteOffer = 0,
  kInvalidRemoteAnswer = 1,
  kInvalidLocalOffer = 2,
  kInvalidLocalAnswer = 3,
  kMaxValue = kInvalidLocalAnswer,
};

// Sharing WebRTC connection event on the sandboxed process.
// Please keep in sync with "SharingWebRtcTimingEvent" in
// src/tools/metrics/histograms/histograms.xml.
enum class WebRtcTimingEvent {
  kInitialized = 0,
  kOfferReceived = 1,
  kIceCandidateReceived = 2,
  kQueuingMessage = 3,
  kSendingMessage = 4,
  kSignalingStable = 5,
  kDataChannelOpen = 6,
  kMessageReceived = 7,
  kAnswerCreated = 8,
  kOfferCreated = 9,
  kAnswerReceived = 10,
  kClosing = 11,
  kClosed = 12,
  kDestroyed = 13,
  kMaxValue = kDestroyed,
};

// Result of receiving a Sharing message via WebRTC.
// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "SharingWebRtcOnMessageReceivedResult" in
// src/tools/metrics/histograms/enums.xml.
enum class WebRtcOnMessageReceivedResult {
  kDecryptionFailed = 0,
  kParseFailed = 1,
  kInvalidPayload = 2,
  kHandlerNotFound = 3,
  kSuccess = 4,
  kMaxValue = kSuccess,
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

// Logs the result of sending a SharingMessage via WebRTC.
void LogWebRtcSendMessageResult(WebRtcSendMessageResult result);

// Logs the error reason for closing WebRTC connection.
void LogWebRtcConnectionErrorReason(WebRtcConnectionErrorReason reason);

// Logs the timing for |event| to UMA.
void LogWebRtcTimingEvent(WebRtcTimingEvent event,
                          base::TimeDelta delay,
                          base::Optional<bool> is_sender);

// Logs the result of receiving a message via WebRTC.
void LogSharingWebRtcOnMessageReceivedResult(
    WebRtcOnMessageReceivedResult result);

}  // namespace sharing

#endif  // CHROME_SERVICES_SHARING_PUBLIC_CPP_SHARING_WEBRTC_METRICS_H_
