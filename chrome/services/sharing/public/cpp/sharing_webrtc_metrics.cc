// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/public/cpp/sharing_webrtc_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace {
// Common prefix for all webrtc metric in Sharing service.
const char kMetricsPrefix[] = "Sharing.WebRtc.";
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
  base::UmaHistogramBoolean(base::StrCat({kMetricsPrefix, "AddIceCandidate"}),
                            success);
}

void LogWebRtcIceConfigFetched(int count) {
  base::UmaHistogramExactLinear(
      base::StrCat({kMetricsPrefix, "IceConfigFetched"}), count,
      /*value_max=*/10);
}

void LogWebRtcTimeout(WebRtcTimeoutState state) {
  base::UmaHistogramEnumeration(base::StrCat({kMetricsPrefix, "Timeout"}),
                                state);
}

void LogWebRtcConnectionType(WebRtcConnectionType type) {
  base::UmaHistogramEnumeration(
      base::StrCat({kMetricsPrefix, "ConnectionType"}), type);
}

}  // namespace sharing
