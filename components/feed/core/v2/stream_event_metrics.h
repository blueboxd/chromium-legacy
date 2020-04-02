// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_STREAM_EVENT_METRICS_H_
#define COMPONENTS_FEED_CORE_V2_STREAM_EVENT_METRICS_H_

#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_stream.h"

namespace feed {

// Reports UMA metrics for stream events.
class StreamEventMetrics : public FeedStream::EventObserver {
 public:
  void OnLoadStream(LoadStreamStatus load_from_store_status,
                    LoadStreamStatus final_status) override;
  void OnMaybeTriggerRefresh(TriggerType trigger,
                             bool clear_all_before_refresh) override;
  void OnClearAll(base::TimeDelta time_since_last_clear) override;
};
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_STREAM_EVENT_METRICS_H_
