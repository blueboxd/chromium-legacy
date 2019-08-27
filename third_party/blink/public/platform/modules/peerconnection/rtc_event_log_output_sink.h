// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PEERCONNECTION_RTC_EVENT_LOG_OUTPUT_SINK_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PEERCONNECTION_RTC_EVENT_LOG_OUTPUT_SINK_H_

#include <string>

#include "third_party/blink/public/platform/web_common.h"

namespace blink {

// TODO(crbug.com/787254): Move this class out of the Blink exposed API
// when all users of it have been Onion souped.
class BLINK_PLATFORM_EXPORT RtcEventLogOutputSink {
 public:
  virtual ~RtcEventLogOutputSink() = default;

  virtual void OnWebRtcEventLogWrite(const std::string& output) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PEERCONNECTION_RTC_EVENT_LOG_OUTPUT_SINK_H_
