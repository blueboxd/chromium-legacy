// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_WEBRTC_SHARING_WEBRTC_TIMING_RECORDER_H_
#define CHROME_SERVICES_SHARING_WEBRTC_SHARING_WEBRTC_TIMING_RECORDER_H_

#include <map>

#include "base/time/time.h"
#include "chrome/services/sharing/public/cpp/sharing_webrtc_metrics.h"

namespace sharing {

// Records event timings in a Sharing WebRTC connection to UMA.
class SharingWebRtcTimingRecorder {
 public:
  SharingWebRtcTimingRecorder();
  SharingWebRtcTimingRecorder(const SharingWebRtcTimingRecorder&) = delete;
  SharingWebRtcTimingRecorder& operator=(const SharingWebRtcTimingRecorder&) =
      delete;
  ~SharingWebRtcTimingRecorder();

  // Logs the first time |event| occurred. The base time depends on |event| and
  // is determined by the |event_start_map| in GetEventStart.
  void LogEvent(WebRtcTimingEvent event);

 private:
  // Map of the timing event to the first timestamp it got logged.
  std::map<WebRtcTimingEvent, base::TimeTicks> timings_;
};

}  // namespace sharing

#endif  // CHROME_SERVICES_SHARING_WEBRTC_SHARING_WEBRTC_TIMING_RECORDER_H_
