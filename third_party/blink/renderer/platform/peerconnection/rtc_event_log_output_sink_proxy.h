// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_EVENT_LOG_OUTPUT_SINK_PROXY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_EVENT_LOG_OUTPUT_SINK_PROXY_H_

#include "third_party/webrtc/api/rtc_event_log_output.h"

namespace blink {

class RtcEventLogOutputSink;

class RtcEventLogOutputSinkProxy final : public webrtc::RtcEventLogOutput {
 public:
  RtcEventLogOutputSinkProxy(RtcEventLogOutputSink* sink);
  ~RtcEventLogOutputSinkProxy() override;

  bool IsActive() const override;

  bool Write(const std::string& output) override;

 private:
  RtcEventLogOutputSink* const sink_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_EVENT_LOG_OUTPUT_SINK_PROXY_H_
