// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PEERCONNECTION_RTC_EVENT_LOG_OUTPUT_SINK_PROXY_UTIL_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PEERCONNECTION_RTC_EVENT_LOG_OUTPUT_SINK_PROXY_UTIL_H_

#include <memory>

#include "third_party/blink/public/platform/web_common.h"

namespace webrtc {
class RtcEventLogOutput;
}

namespace blink {

class RtcEventLogOutputSink;

// TODO(crbug.com/787254): Remove this API when its clients are Onion souped.
BLINK_PLATFORM_EXPORT std::unique_ptr<webrtc::RtcEventLogOutput>
CreateRtcEventLogOutputSinkProxy(RtcEventLogOutputSink* sink);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PEERCONNECTION_RTC_EVENT_LOG_OUTPUT_SINK_PROXY_UTIL_H_
