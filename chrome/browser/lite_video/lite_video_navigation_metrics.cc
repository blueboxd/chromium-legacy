// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_navigation_metrics.h"

namespace lite_video {

LiteVideoNavigationMetrics::LiteVideoNavigationMetrics(
    int64_t nav_id,
    LiteVideoDecision decision,
    LiteVideoBlocklistReason blocklist_reason)
    : nav_id_(nav_id),
      decision_(decision),
      blocklist_reason_(blocklist_reason) {}

LiteVideoNavigationMetrics::~LiteVideoNavigationMetrics() = default;

LiteVideoBlocklistReason LiteVideoNavigationMetrics::blocklist_reason() const {
  return blocklist_reason_;
}

}  // namespace lite_video
