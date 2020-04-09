// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/feed/core/v2/metrics_reporter.h"

#include <cmath>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"

namespace feed {
namespace {
using feed::internal::FeedEngagementType;

void ReportEngagementTypeHistogram(FeedEngagementType engagement_type) {
  UMA_HISTOGRAM_ENUMERATION("ContentSuggestions.Feed.EngagementType",
                            engagement_type);
}

}  // namespace

MetricsReporter::MetricsReporter(const base::TickClock* clock)
    : clock_(clock) {}
MetricsReporter::~MetricsReporter() = default;

// Engagement Tracking.

void MetricsReporter::RecordInteraction() {
  RecordEngagement(/*scroll_distance_dp=*/0, /*interacted=*/true);
  ReportEngagementTypeHistogram(FeedEngagementType::kFeedInteracted);
}

void MetricsReporter::RecordEngagement(int scroll_distance_dp,
                                       bool interacted) {
  scroll_distance_dp = std::abs(scroll_distance_dp);
  // Determine if this interaction is part of a new 'session'.
  auto now = clock_->NowTicks();
  const base::TimeDelta kVisitTimeout = base::TimeDelta::FromMinutes(5);
  if (now - visit_start_time_ > kVisitTimeout) {
    engaged_reported_ = false;
    engaged_simple_reported_ = false;
  }
  // Reset the last active time for session measurement.
  visit_start_time_ = now;

  // Report the user as engaged-simple if they have scrolled any amount or
  // interacted with the card, and we have not already reported it for this
  // chrome run.
  if (!engaged_simple_reported_ && (scroll_distance_dp > 0 || interacted)) {
    ReportEngagementTypeHistogram(FeedEngagementType::kFeedEngagedSimple);
    engaged_simple_reported_ = true;
  }

  // Report the user as engaged if they have scrolled more than the threshold or
  // interacted with the card, and we have not already reported it this chrome
  // run.
  const int kMinScrollThresholdDp = 160;  // 1 inch.
  if (!engaged_reported_ &&
      (scroll_distance_dp > kMinScrollThresholdDp || interacted)) {
    ReportEngagementTypeHistogram(FeedEngagementType::kFeedEngaged);
    engaged_reported_ = true;
  }
}

void MetricsReporter::StreamScrolled(int distance_dp) {
  RecordEngagement(distance_dp, /*interacted=*/false);

  if (!scrolled_reported_) {
    ReportEngagementTypeHistogram(FeedEngagementType::kFeedScrolled);
    scrolled_reported_ = true;
  }
}

void MetricsReporter::NavigationStarted() {
  // TODO(harringtond): Add user actions.
  // Report Feed_OpeningContent
  RecordInteraction();
}

void MetricsReporter::NavigationDone() {
  // TODO(harringtond): Use this or remove it.
}

void MetricsReporter::ContentRemoved() {
  // TODO(harringtond): Add user actions.
  // Report Feed_RemovedContent
  RecordInteraction();
}

void MetricsReporter::NotInterestedIn() {
  // TODO(harringtond): Add user actions.
  // Report Feed_NotInterestedIn
  RecordInteraction();
}

void MetricsReporter::ManageInterests() {
  // TODO(harringtond): Add user actions.
  // Report Feed_ManageInterests
  RecordInteraction();
}

void MetricsReporter::ContextMenuOpened() {
  // TODO(harringtond): Add user actions.
  // Report Feed_OpenedContextMenu
}

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
