// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_event_log_output_sink_proxy.h"

#include "base/logging.h"
#include "third_party/blink/public/platform/modules/peerconnection/rtc_event_log_output_sink.h"
#include "third_party/blink/public/platform/modules/peerconnection/rtc_event_log_output_sink_proxy_util.h"

namespace blink {

std::unique_ptr<webrtc::RtcEventLogOutput> CreateRtcEventLogOutputSinkProxy(
    RtcEventLogOutputSink* sink) {
  return std::make_unique<RtcEventLogOutputSinkProxy>(sink);
}

RtcEventLogOutputSinkProxy::RtcEventLogOutputSinkProxy(
    RtcEventLogOutputSink* sink)
    : sink_(sink) {
  CHECK(sink_);
}

RtcEventLogOutputSinkProxy::~RtcEventLogOutputSinkProxy() = default;

bool RtcEventLogOutputSinkProxy::IsActive() const {
  return true;  // Active until the proxy is destroyed.
}

bool RtcEventLogOutputSinkProxy::Write(const std::string& output) {
  sink_->OnWebRtcEventLogWrite(output);
  return true;
}

}  // namespace blink
