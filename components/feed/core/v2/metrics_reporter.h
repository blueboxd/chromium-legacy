// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_METRICS_REPORTER_H_
#define COMPONENTS_FEED_CORE_V2_METRICS_REPORTER_H_

#include "base/time/time.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_stream.h"

namespace base {
class TickClock;
}  // namespace base

namespace feed {

namespace internal {
// This enum is used for a UMA histogram. Keep in sync with FeedEngagementType
// in enums.xml.
enum class FeedEngagementType {
  kFeedEngaged = 0,
  kFeedEngagedSimple = 1,
  kFeedInteracted = 2,
  kFeedScrolled = 3,
  kMaxValue = FeedEngagementType::kFeedScrolled,
};
}  // namespace internal

// Reports UMA metrics for feed.
class MetricsReporter : public FeedStream::EventObserver {
 public:
  explicit MetricsReporter(const base::TickClock* clock);
  virtual ~MetricsReporter();
  MetricsReporter(const MetricsReporter&) = delete;
  MetricsReporter& operator=(const MetricsReporter&) = delete;

  // User interactions.

  void NavigationStarted();
  void NavigationDone();
  void ContentRemoved();
  void NotInterestedIn();
  void ManageInterests();
  void ContextMenuOpened();
  // Indicates the user scrolled the feed by |distance_dp| and then stopped
  // scrolling.
  void StreamScrolled(int distance_dp);

  // Network metrics.

  static void NetworkRequestComplete(NetworkRequestType type,
                                     int http_status_code);

  // FeedStream::EventObserver.

  void OnLoadStream(LoadStreamStatus load_from_store_status,
                    LoadStreamStatus final_status) override;
  void OnMaybeTriggerRefresh(TriggerType trigger,
                             bool clear_all_before_refresh) override;
  void OnClearAll(base::TimeDelta time_since_last_clear) override;

 private:
  void RecordEngagement(int scroll_distance_dp, bool interacted);
  void RecordInteraction();

  const base::TickClock* clock_;

  base::TimeTicks visit_start_time_;
  bool engaged_simple_reported_ = false;
  bool engaged_reported_ = false;
  bool scrolled_reported_ = false;
};
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_METRICS_REPORTER_H_
