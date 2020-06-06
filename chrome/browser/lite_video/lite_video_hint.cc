// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_hint.h"

namespace lite_video {

LiteVideoHint::LiteVideoHint(int target_downlink_bandwidth_kbps,
                             int target_downlink_rtt_latency_ms,
                             int kilobytes_to_buffer_before_throttle)
    : target_downlink_bandwidth_kbps_(target_downlink_bandwidth_kbps),
      target_downlink_rtt_latency_ms_(target_downlink_rtt_latency_ms),
      kilobytes_to_buffer_before_throttle_(
          kilobytes_to_buffer_before_throttle) {}

}  // namespace lite_video
