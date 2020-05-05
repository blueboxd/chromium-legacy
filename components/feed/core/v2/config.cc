// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/config.h"

#include "base/metrics/field_trial_params.h"
#include "components/feed/feed_feature_list.h"

namespace feed {
namespace {
// A note about the design.
// Soon, we'll add the ability to override configuration values from sources
// other than Finch. Finch serves well for experimentation, but after we're done
// experimenting, we still want to control some of these values. The tentative
// plan is to send configuration down from the server, and store it in prefs.
// The source of a config value would be the following, in order of preference:
// finch, server, default-value.
Config g_config;

// Override any parameters that may be provided by Finch.
void OverrideWithFinch(Config* config) {
  config->max_feed_query_requests_per_day =
      base::GetFieldTrialParamByFeatureAsInt(
          kInterestFeedV2, "max_feed_query_requests_per_day",
          config->max_feed_query_requests_per_day);

  config->max_action_upload_requests_per_day =
      base::GetFieldTrialParamByFeatureAsInt(
          kInterestFeedV2, "max_action_upload_requests_per_day",
          config->max_action_upload_requests_per_day);

  config->stale_content_threshold =
      base::TimeDelta::FromSecondsD(base::GetFieldTrialParamByFeatureAsDouble(
          kInterestFeedV2, "stale_content_threshold_seconds",
          config->stale_content_threshold.InSecondsF()));

  config->default_background_refresh_interval =
      base::TimeDelta::FromSecondsD(base::GetFieldTrialParamByFeatureAsDouble(
          kInterestFeedV2, "default_background_refresh_interval_seconds",
          config->stale_content_threshold.InSecondsF()));
}

}  // namespace

const Config& GetFeedConfig() {
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    OverrideWithFinch(&g_config);
  }
  return g_config;
}

void SetFeedConfigForTesting(const Config& config) {
  g_config = config;
}

}  // namespace feed
