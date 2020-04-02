// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/feed/core/v2/stream_event_metrics.h"

#include "base/metrics/histogram_macros.h"

namespace feed {

void StreamEventMetrics::OnLoadStream(LoadStreamStatus load_from_store_status,
                                      LoadStreamStatus final_status) {
  // TODO(harringtond): Add UMA for this, or record it with another histogram.
}

void StreamEventMetrics::OnMaybeTriggerRefresh(TriggerType trigger,
                                               bool clear_all_before_refresh) {
  // TODO(harringtond): Either add UMA for this or remove it.
}

void StreamEventMetrics::OnClearAll(base::TimeDelta time_since_last_clear) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "ContentSuggestions.Feed.Scheduler.TimeSinceLastFetchOnClear",
      time_since_last_clear, base::TimeDelta::FromSeconds(1),
      base::TimeDelta::FromDays(7),
      /*bucket_count=*/50);
}

}  // namespace feed
