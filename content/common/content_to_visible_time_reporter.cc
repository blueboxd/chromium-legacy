// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_to_visible_time_reporter.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/presentation_feedback.h"

namespace content {

namespace {

bool IsOptionalValueTrue(const base::Optional<bool>& data) {
  return data.has_value() && data.value();
}

//  Used to generate unique "TabSwitching::Latency" event ids. Note: The address
//  of ContentToVisibleTimeReporter can't be used as an id because a single
//  ContentToVisibleTimeReporter can generate multiple overlapping events.
int g_num_trace_events_in_process = 0;

const char* GetHistogramSuffix(
    bool has_saved_frames,
    const RecordContentToVisibleTimeRequest& start_state) {
  if (has_saved_frames)
    return "WithSavedFrames";

  if (IsOptionalValueTrue(start_state.destination_is_loaded)) {
    if (IsOptionalValueTrue(start_state.destination_is_frozen)) {
      return "NoSavedFrames_Loaded_Frozen";
    } else {
      return "NoSavedFrames_Loaded_NotFrozen";
    }
  } else {
    return "NoSavedFrames_NotLoaded";
  }
}

void ReportUnOccludedMetric(const base::TimeTicks requested_time,
                            const gfx::PresentationFeedback& feedback) {
  const base::TimeDelta delta = feedback.timestamp - requested_time;
  UMA_HISTOGRAM_TIMES("Aura.WebContentsWindowUnOccludedTime", delta);
}

void RecordBackForwardCacheRestoreMetric(
    const base::TimeTicks requested_time,
    const gfx::PresentationFeedback& feedback) {
  const base::TimeDelta delta = feedback.timestamp - requested_time;
  // Histogram to record the content to visible duration after restoring a page
  // from back-forward cache. Here min, max bucket size are same as the
  // "PageLoad.PaintTiming.NavigationToFirstContentfulPaint" metric.
  base::UmaHistogramCustomTimes(
      "BackForwardCache.Restore.NavigationToFirstPaint", delta,
      base::TimeDelta::FromMilliseconds(10), base::TimeDelta::FromMinutes(10),
      100);
}

}  // namespace

RecordContentToVisibleTimeRequest::RecordContentToVisibleTimeRequest() =
    default;

RecordContentToVisibleTimeRequest::~RecordContentToVisibleTimeRequest() =
    default;

RecordContentToVisibleTimeRequest::RecordContentToVisibleTimeRequest(
    const RecordContentToVisibleTimeRequest& other) = default;

RecordContentToVisibleTimeRequest::RecordContentToVisibleTimeRequest(
    base::TimeTicks event_start_time,
    base::Optional<bool> destination_is_loaded,
    base::Optional<bool> destination_is_frozen,
    bool show_reason_tab_switching,
    bool show_reason_unoccluded,
    bool show_reason_bfcache_restore)
    : event_start_time(event_start_time),
      destination_is_loaded(destination_is_loaded),
      destination_is_frozen(destination_is_frozen),
      show_reason_tab_switching(show_reason_tab_switching),
      show_reason_unoccluded(show_reason_unoccluded),
      show_reason_bfcache_restore(show_reason_bfcache_restore) {}

void RecordContentToVisibleTimeRequest::UpdateRequest(
    const RecordContentToVisibleTimeRequest& other) {
  event_start_time = std::min(event_start_time, other.event_start_time);
  if (IsOptionalValueTrue(other.destination_is_loaded))
    destination_is_loaded = other.destination_is_loaded;

  if (IsOptionalValueTrue(other.destination_is_frozen))
    destination_is_frozen = other.destination_is_frozen;

  show_reason_tab_switching |= other.show_reason_tab_switching;
  show_reason_unoccluded |= other.show_reason_unoccluded;
  show_reason_bfcache_restore |= other.show_reason_bfcache_restore;
}

ContentToVisibleTimeReporter::ContentToVisibleTimeReporter() = default;

ContentToVisibleTimeReporter::~ContentToVisibleTimeReporter() = default;

base::OnceCallback<void(const gfx::PresentationFeedback&)>
ContentToVisibleTimeReporter::TabWasShown(
    bool has_saved_frames,
    const RecordContentToVisibleTimeRequest& start_state,
    base::TimeTicks render_widget_visibility_request_timestamp) {
  DCHECK(!start_state.event_start_time.is_null());
  DCHECK(!render_widget_visibility_request_timestamp.is_null());
  DCHECK(!tab_switch_start_state_);
  DCHECK(render_widget_visibility_request_timestamp_.is_null());

  has_saved_frames_ = has_saved_frames;
  tab_switch_start_state_ = start_state;
  render_widget_visibility_request_timestamp_ =
      render_widget_visibility_request_timestamp;

  // |tab_switch_start_state_| is only reset by RecordHistogramsAndTraceEvents
  // once the metrics have been emitted.
  return base::BindOnce(
      &ContentToVisibleTimeReporter::RecordHistogramsAndTraceEvents,
      weak_ptr_factory_.GetWeakPtr(), false /* is_incomplete */,
      start_state.show_reason_tab_switching, start_state.show_reason_unoccluded,
      start_state.show_reason_bfcache_restore);
}

void ContentToVisibleTimeReporter::TabWasHidden() {
  if (tab_switch_start_state_) {
    RecordHistogramsAndTraceEvents(true /* is_incomplete */,
                                   true /* show_reason_tab_switching */,
                                   false /* show_reason_unoccluded */,
                                   false /* show_reason_bfcache_restore */,
                                   gfx::PresentationFeedback::Failure());
    weak_ptr_factory_.InvalidateWeakPtrs();
  }
}

void ContentToVisibleTimeReporter::RecordHistogramsAndTraceEvents(
    bool is_incomplete,
    bool show_reason_tab_switching,
    bool show_reason_unoccluded,
    bool show_reason_bfcache_restore,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(tab_switch_start_state_);
  DCHECK(!render_widget_visibility_request_timestamp_.is_null());
  // If the DCHECK fail, make sure RenderWidgetHostImpl::WasShown was triggered
  // for recording the event.
  DCHECK(show_reason_bfcache_restore || show_reason_unoccluded ||
         show_reason_tab_switching);

  if (show_reason_bfcache_restore) {
    RecordBackForwardCacheRestoreMetric(
        tab_switch_start_state_->event_start_time, feedback);
  }

  if (show_reason_unoccluded) {
    ReportUnOccludedMetric(tab_switch_start_state_->event_start_time, feedback);
  }

  if (!show_reason_tab_switching)
    return;

  // Tab switching has occurred.
  auto tab_switch_result = TabSwitchResult::kSuccess;
  if (is_incomplete)
    tab_switch_result = TabSwitchResult::kIncomplete;
  else if (feedback.flags & gfx::PresentationFeedback::kFailure)
    tab_switch_result = TabSwitchResult::kPresentationFailure;

  const auto tab_switch_duration =
      feedback.timestamp - tab_switch_start_state_->event_start_time;

  // Record trace events.
  TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "latency", "TabSwitching::Latency",
      TRACE_ID_LOCAL(g_num_trace_events_in_process),
      tab_switch_start_state_->event_start_time);
  TRACE_EVENT_ASYNC_END_WITH_TIMESTAMP2(
      "latency", "TabSwitching::Latency",
      TRACE_ID_LOCAL(g_num_trace_events_in_process), feedback.timestamp,
      "result", tab_switch_result, "latency",
      tab_switch_duration.InMillisecondsF());
  ++g_num_trace_events_in_process;

  // Each value recorded to a histogram with suffix .NoSavedFrames_Loaded_Frozen
  // or .NoSavedFrames_Loaded_NotFrozen is also recorded to a histogram with
  // suffix .NoSavedFrames_Loaded to facilitate assessing the impact of
  // experiments that affect the number of frozen tab on tab switch time.
  const bool is_no_saved_frames_loaded =
      !has_saved_frames_ &&
      IsOptionalValueTrue(
          tab_switch_start_state_.value().destination_is_loaded);

  // Record result histogram.
  base::UmaHistogramEnumeration(
      std::string("Browser.Tabs.TabSwitchResult.") +
          GetHistogramSuffix(has_saved_frames_,
                             tab_switch_start_state_.value()),
      tab_switch_result);

  if (is_no_saved_frames_loaded) {
    UMA_HISTOGRAM_ENUMERATION(
        "Browser.Tabs.TabSwitchResult.NoSavedFrames_Loaded", tab_switch_result);
  }

  // Record latency histogram.
  switch (tab_switch_result) {
    case TabSwitchResult::kSuccess: {
      base::UmaHistogramTimes(
          std::string("Browser.Tabs.TotalSwitchDuration.") +
              GetHistogramSuffix(has_saved_frames_,
                                 tab_switch_start_state_.value()),
          tab_switch_duration);

      if (is_no_saved_frames_loaded) {
        UMA_HISTOGRAM_TIMES(
            "Browser.Tabs.TotalSwitchDuration.NoSavedFrames_Loaded",
            tab_switch_duration);
      }
      break;
    }
    case TabSwitchResult::kIncomplete: {
      base::UmaHistogramTimes(
          std::string("Browser.Tabs.TotalIncompleteSwitchDuration.") +
              GetHistogramSuffix(has_saved_frames_,
                                 tab_switch_start_state_.value()),
          tab_switch_duration);

      if (is_no_saved_frames_loaded) {
        UMA_HISTOGRAM_TIMES(
            "Browser.Tabs.TotalIncompleteSwitchDuration.NoSavedFrames_Loaded",
            tab_switch_duration);
      }
      break;
    }
    case TabSwitchResult::kPresentationFailure: {
      break;
    }
  }

  // Record legacy latency histogram.
  UMA_HISTOGRAM_TIMES(
      "MPArch.RWH_TabSwitchPaintDuration",
      feedback.timestamp - render_widget_visibility_request_timestamp_);

  // Reset tab switch information.
  has_saved_frames_ = false;
  tab_switch_start_state_.reset();
  render_widget_visibility_request_timestamp_ = base::TimeTicks();
}

}  // namespace content
