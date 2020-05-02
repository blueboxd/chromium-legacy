// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_CONFIG_H_
#define COMPONENTS_FEED_CORE_V2_CONFIG_H_

namespace feed {

// The Feed configuration. Default values appear below. Always use
// |GetFeedConfig()| to get the current configuration.
struct Config {
  // Maximum number of FeedQuery or action upload requests per day.
  int max_feed_query_requests_per_day = 20;
  int max_action_upload_requests_per_day = 20;
};

// Gets the current configuration.
const Config& GetFeedConfig();

void SetFeedConfigForTesting(const Config& config);

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_CONFIG_H_
