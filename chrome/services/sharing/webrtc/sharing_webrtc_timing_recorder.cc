// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/webrtc/sharing_webrtc_timing_recorder.h"

namespace {

// Gets the event from which we want to log the time until |event|.
sharing::WebRtcTimingEvent GetEventStart(sharing::WebRtcTimingEvent event) {
  static const std::map<sharing::WebRtcTimingEvent, sharing::WebRtcTimingEvent>
      event_start_map = {
          {sharing::WebRtcTimingEvent::kSendingMessage,
           sharing::WebRtcTimingEvent::kQueuingMessage},
          {sharing::WebRtcTimingEvent::kDataChannelOpen,
           sharing::WebRtcTimingEvent::kSignalingStable},
          {sharing::WebRtcTimingEvent::kMessageReceived,
           sharing::WebRtcTimingEvent::kDataChannelOpen},
          {sharing::WebRtcTimingEvent::kClosed,
           sharing::WebRtcTimingEvent::kClosing},
          {sharing::WebRtcTimingEvent::kAnswerReceived,
           sharing::WebRtcTimingEvent::kOfferCreated},
          {sharing::WebRtcTimingEvent::kAnswerCreated,
           sharing::WebRtcTimingEvent::kOfferReceived},
      };

  const auto result = event_start_map.find(event);
  if (result == event_start_map.end())
    return sharing::WebRtcTimingEvent::kInitialized;
  return result->second;
}

}  // namespace

namespace sharing {

SharingWebRtcTimingRecorder::SharingWebRtcTimingRecorder() {
  timings_[WebRtcTimingEvent::kInitialized] = base::TimeTicks::Now();
}

SharingWebRtcTimingRecorder::~SharingWebRtcTimingRecorder() {
  LogEvent(WebRtcTimingEvent::kDestroyed);
}

void SharingWebRtcTimingRecorder::LogEvent(WebRtcTimingEvent event) {
  auto from_iter = timings_.find(GetEventStart(event));
  if (from_iter == timings_.end())
    return;

  auto to_iter = timings_.emplace(event, base::TimeTicks::Now());
  if (!to_iter.second)
    return;

  base::TimeDelta delay = to_iter.first->second - from_iter->second;
  LogWebRtcTimingEvent(event, delay, is_sender_);
}

}  // namespace sharing
