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

// This enum must match FeedUserActionType in enums.xml.
// TODO(harringtond): Add user actions for most of these.
enum class FeedUserActionType {
  kTappedOnCard = 0,
  kShownCard = 1,
  kTappedSendFeedback = 2,
  kTappedLearnMore = 3,
  kTappedHideStory = 4,
  kTappedNotInterestedIn = 5,
  kTappedManageInterests = 6,
  kTappedDownload = 7,
  kTappedOpenInNewTab = 8,
  kOpenedContextMenu = 9,
  kOpenedFeedSurface = 10,
  kMaxValue = kOpenedFeedSurface,
};

void ReportEngagementTypeHistogram(FeedEngagementType engagement_type) {
  UMA_HISTOGRAM_ENUMERATION("ContentSuggestions.Feed.EngagementType",
                            engagement_type);
}

void ReportUserActionHistogram(FeedUserActionType action_type) {
  UMA_HISTOGRAM_ENUMERATION("ContentSuggestions.Feed.UserAction", action_type);
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

void MetricsReporter::ContentSliceViewed(int index_in_stream) {
  const int kMaxSuggestionsTotal = 50;
  UMA_HISTOGRAM_EXACT_LINEAR("NewTabPage.ContentSuggestions.Shown",
                             index_in_stream, kMaxSuggestionsTotal);
}

void MetricsReporter::OpenAction() {
  ReportUserActionHistogram(FeedUserActionType::kTappedOnCard);
  RecordInteraction();
}

void MetricsReporter::OpenInNewTabAction() {
  ReportUserActionHistogram(FeedUserActionType::kTappedOpenInNewTab);
  RecordInteraction();
}

void MetricsReporter::SendFeedbackAction() {
  ReportUserActionHistogram(FeedUserActionType::kTappedSendFeedback);
  RecordInteraction();
}

void MetricsReporter::DownloadAction() {
  ReportUserActionHistogram(FeedUserActionType::kTappedDownload);
  RecordInteraction();
}

void MetricsReporter::LearnMoreAction() {
  ReportUserActionHistogram(FeedUserActionType::kTappedLearnMore);
  RecordInteraction();
}

void MetricsReporter::NavigationStarted() {
  // TODO(harringtond): Use this or remove it.
}

void MetricsReporter::NavigationDone() {
  // TODO(harringtond): Use this or remove it.
}

void MetricsReporter::RemoveAction() {
  ReportUserActionHistogram(FeedUserActionType::kTappedHideStory);
  RecordInteraction();
}

void MetricsReporter::NotInterestedInAction() {
  ReportUserActionHistogram(FeedUserActionType::kTappedNotInterestedIn);
  RecordInteraction();
}

void MetricsReporter::ManageInterestsAction() {
  ReportUserActionHistogram(FeedUserActionType::kTappedManageInterests);
  RecordInteraction();
}

void MetricsReporter::ContextMenuOpened() {
  ReportUserActionHistogram(FeedUserActionType::kOpenedContextMenu);
}

void MetricsReporter::SurfaceOpened() {
  ReportUserActionHistogram(FeedUserActionType::kOpenedFeedSurface);
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
  UMA_HISTOGRAM_ENUMERATION("ContentSuggestions.Feed.LoadStreamStatus.Initial",
                            final_status);
  if (load_from_store_status != LoadStreamStatus::kNoStatus) {
    UMA_HISTOGRAM_ENUMERATION(
        "ContentSuggestions.Feed.LoadStreamStatus.InitialFromStore",
        load_from_store_status);
  }
}

void MetricsReporter::OnBackgroundRefresh(LoadStreamStatus final_status) {
  UMA_HISTOGRAM_ENUMERATION(
      "ContentSuggestions.Feed.LoadStreamStatus.BackgroundRefresh",
      final_status);
}

void MetricsReporter::OnLoadMore(LoadStreamStatus final_status) {
  UMA_HISTOGRAM_ENUMERATION("ContentSuggestions.Feed.LoadStreamStatus.LoadMore",
                            final_status);
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
