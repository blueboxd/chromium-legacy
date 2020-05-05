// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_CONFIG_H_
#define COMPONENTS_FEED_CORE_V2_CONFIG_H_

#include "base/time/time.h"

namespace feed {

// The Feed configuration. Default values appear below. Always use
// |GetFeedConfig()| to get the current configuration.
struct Config {
  // Maximum number of FeedQuery or action upload requests per day.
  int max_feed_query_requests_per_day = 20;
  int max_action_upload_requests_per_day = 20;
  // Content older than this threshold will not be shown to the user.
  base::TimeDelta stale_content_threshold = base::TimeDelta::FromHours(48);
  // The time between background refresh attempts. Ignored if a server-defined
  // fetch schedule has been assigned.
  base::TimeDelta default_background_refresh_interval =
      base::TimeDelta::FromHours(24);
};

// Gets the current configuration.
const Config& GetFeedConfig();

void SetFeedConfigForTesting(const Config& config);

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_CONFIG_H_
