// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_HINT_H_
#define CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_HINT_H_

#include <stdint.h>

namespace lite_video {

class LiteVideoHint {
 public:
  LiteVideoHint(int target_downlink_bandwidth_kbps,
                int target_downlink_rtt_latency_ms,
                int kilobytes_to_buffer_before_throttle);
  ~LiteVideoHint() = default;

  int target_downlink_bandwidth_kbps() {
    return target_downlink_bandwidth_kbps_;
  }

  int target_downlink_rtt_latency_ms() {
    return target_downlink_rtt_latency_ms_;
  }

  int kilobytes_to_buffer_before_throttle() {
    return kilobytes_to_buffer_before_throttle_;
  }

 private:
  const int target_downlink_bandwidth_kbps_;
  const int target_downlink_rtt_latency_ms_;
  const int kilobytes_to_buffer_before_throttle_;
};

}  // namespace lite_video

#endif  // CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_HINT_H_
