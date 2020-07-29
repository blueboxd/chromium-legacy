// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_NAVIGATION_METRICS_H_
#define CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_NAVIGATION_METRICS_H_

#include "chrome/browser/lite_video/lite_video_user_blocklist.h"

namespace lite_video {

// The decision if a navigation should attempt to throttle media requests.
// This should be kept in sync with LiteVideoDecision in enums.xml.
enum class LiteVideoDecision {
  kUnknown,
  // The navigation is allowed by all types of this LiteVideoUserBlocklist.
  kAllowed,
  // The navigation is not allowed by all types of this LiteVideoUserBlocklist.
  kNotAllowed,
  // The navigation is allowed by all types of this LiteVideoUserBlocklist but
  // the optimization was heldback for counterfactual experiments.
  kHoldback,

  // Insert new values before this line.
  kMaxValue = kHoldback,
};

class LiteVideoNavigationMetrics {
 public:
  LiteVideoNavigationMetrics(int64_t nav_id,
                             LiteVideoDecision decision,
                             LiteVideoBlocklistReason blocklist_reason);
  ~LiteVideoNavigationMetrics();

  int64_t nav_id() const { return nav_id_; }
  LiteVideoDecision decision() const { return decision_; }
  LiteVideoBlocklistReason blocklist_reason() const;

 private:
  int64_t nav_id_;
  LiteVideoDecision decision_;
  LiteVideoBlocklistReason blocklist_reason_;
};

}  // namespace lite_video

#endif  // CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_NAVIGATION_METRICS_H_
