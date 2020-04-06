// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/feed/core/v2/metrics_reporter.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace feed {

void MetricsReporter::NetworkRequestComplete(NetworkRequestType type,
                                             int http_status_code) {
  switch (type) {
    case NetworkRequestType::kFeedQuery:
      base::UmaHistogramSparse(
          "ContentSuggestions.Feed.Network.ResponseStatus.FeedQuery",
          http_status_code);
      return;
    case NetworkRequestType::kUploadActions:
      base::UmaHistogramSparse(
          "ContentSuggestions.Feed.Network.ResponseStatus.UploadActions",
          http_status_code);
      return;
  }
}

void MetricsReporter::OnLoadStream(LoadStreamStatus load_from_store_status,
                                   LoadStreamStatus final_status) {
  // TODO(harringtond): Add UMA for this, or record it with another histogram.
}

void MetricsReporter::OnMaybeTriggerRefresh(TriggerType trigger,
                                            bool clear_all_before_refresh) {
  // TODO(harringtond): Either add UMA for this or remove it.
}

void MetricsReporter::OnClearAll(base::TimeDelta time_since_last_clear) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "ContentSuggestions.Feed.Scheduler.TimeSinceLastFetchOnClear",
      time_since_last_clear, base::TimeDelta::FromSeconds(1),
      base::TimeDelta::FromDays(7),
      /*bucket_count=*/50);
}

}  // namespace feed
