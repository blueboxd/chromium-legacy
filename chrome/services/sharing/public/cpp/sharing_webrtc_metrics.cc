// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/public/cpp/sharing_webrtc_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"

namespace {

// Common prefix for all webrtc metric in Sharing service.
const char kMetricsPrefix[] = "Sharing.WebRtc";

// Return values must be kept in sync with "SharingWebRtcTimingEvent" in
// src/tools/metrics/histograms/histograms.xml.
std::string TimingEventToString(sharing::WebRtcTimingEvent event) {
  switch (event) {
    case sharing::WebRtcTimingEvent::kInitialized:
      return "Initialized";
    case sharing::WebRtcTimingEvent::kOfferReceived:
      return "OfferReceived";
    case sharing::WebRtcTimingEvent::kIceCandidateReceived:
      return "IceCandidateReceived";
    case sharing::WebRtcTimingEvent::kQueuingMessage:
      return "QueuingMessage";
    case sharing::WebRtcTimingEvent::kSendingMessage:
      return "SendingMessage";
    case sharing::WebRtcTimingEvent::kSignalingStable:
      return "SignalingStable";
    case sharing::WebRtcTimingEvent::kDataChannelOpen:
      return "DataChannelOpen";
    case sharing::WebRtcTimingEvent::kMessageReceived:
      return "MessageReceived";
    case sharing::WebRtcTimingEvent::kAnswerCreated:
      return "AnswerCreated";
    case sharing::WebRtcTimingEvent::kOfferCreated:
      return "OfferCreated";
    case sharing::WebRtcTimingEvent::kAnswerReceived:
      return "AnswerReceived";
    case sharing::WebRtcTimingEvent::kClosing:
      return "Closing";
    case sharing::WebRtcTimingEvent::kClosed:
      return "Closed";
    case sharing::WebRtcTimingEvent::kDestroyed:
      return "Destroyed";
  }
}

// Return values must be kept in sync with "SharingWebRtcTimingEventRole" in
// src/tools/metrics/histograms/histograms.xml.
std::string SenderReceiverSuffix(base::Optional<bool> is_sender) {
  if (!is_sender.has_value())
    return "Unknown";

  return *is_sender ? "Sender" : "Receiver";
}

}  // namespace

namespace sharing {

sharing::WebRtcConnectionType StringToWebRtcConnectionType(
    const std::string& type) {
  if (type == "host")
    return sharing::WebRtcConnectionType::kHost;

  if (type == "srflx")
    return sharing::WebRtcConnectionType::kServerReflexive;

  if (type == "prflx")
    return sharing::WebRtcConnectionType::kPeerReflexive;

  if (type == "relay")
    return sharing::WebRtcConnectionType::kRelay;

  return sharing::WebRtcConnectionType::kUnknown;
}

void LogWebRtcAddIceCandidate(bool success) {
  base::UmaHistogramBoolean(
      base::JoinString({kMetricsPrefix, "AddIceCandidate"}, "."), success);
}

void LogWebRtcIceConfigFetched(int count) {
  base::UmaHistogramExactLinear(
      base::JoinString({kMetricsPrefix, "IceConfigFetched"}, "."), count,
      /*value_max=*/10);
}

void LogWebRtcTimeout(WebRtcTimeoutState state) {
  base::UmaHistogramEnumeration(
      base::JoinString({kMetricsPrefix, "Timeout"}, "."), state);
}

void LogWebRtcConnectionType(WebRtcConnectionType type) {
  base::UmaHistogramEnumeration(
      base::JoinString({kMetricsPrefix, "ConnectionType"}, "."), type);
}

void LogWebRtcSendMessageResult(WebRtcSendMessageResult result) {
  base::UmaHistogramEnumeration(
      base::JoinString({kMetricsPrefix, "SendMessageResult"}, "."), result);
}

void LogWebRtcConnectionErrorReason(WebRtcConnectionErrorReason reason) {
  base::UmaHistogramEnumeration(
      base::JoinString({kMetricsPrefix, "ConnectionErrorReason"}, "."), reason);
}

void LogWebRtcTimingEvent(WebRtcTimingEvent event,
                          base::TimeDelta delay,
                          base::Optional<bool> is_sender) {
  base::UmaHistogramMediumTimes(
      base::JoinString(
          {kMetricsPrefix, "TimingEvents", TimingEventToString(event)}, "."),
      delay);

  base::UmaHistogramMediumTimes(
      base::JoinString(
          {kMetricsPrefix, "TimingEvents", SenderReceiverSuffix(is_sender),
           TimingEventToString(event)},
          "."),
      delay);
}

void LogSharingWebRtcOnMessageReceivedResult(
    WebRtcOnMessageReceivedResult result) {
  base::UmaHistogramEnumeration(
      base::JoinString({kMetricsPrefix, "OnMessageReceivedResult"}, "."),
      result);
}

}  // namespace sharing
