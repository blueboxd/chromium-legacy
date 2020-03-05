// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/popup_tracker.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/ui/blocked_content/popup_opener_tab_helper.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

PopupTracker* PopupTracker::CreateForWebContents(content::WebContents* contents,
                                                 content::WebContents* opener) {
  DCHECK(contents);
  DCHECK(opener);
  auto* tracker = FromWebContents(contents);
  if (!tracker) {
    tracker = new PopupTracker(contents, opener);
    contents->SetUserData(UserDataKey(), base::WrapUnique(tracker));
  }
  return tracker;
}

PopupTracker::~PopupTracker() = default;

PopupTracker::PopupTracker(content::WebContents* contents,
                           content::WebContents* opener)
    : content::WebContentsObserver(contents),
      visibility_tracker_(
          base::DefaultTickClock::GetInstance(),
          contents->GetVisibility() != content::Visibility::HIDDEN),
      opener_source_id_(ukm::GetSourceIdForWebContentsDocument(opener)) {
  if (auto* popup_opener = PopupOpenerTabHelper::FromWebContents(opener))
    popup_opener->OnOpenedPopup(this);
}

void PopupTracker::WebContentsDestroyed() {
  base::TimeDelta total_foreground_duration =
      visibility_tracker_.GetForegroundDuration();
  if (first_load_visible_time_start_) {
    base::TimeDelta first_load_visible_time =
        first_load_visible_time_
            ? *first_load_visible_time_
            : total_foreground_duration - *first_load_visible_time_start_;
    UMA_HISTOGRAM_LONG_TIMES(
        "ContentSettings.Popups.FirstDocumentEngagementTime2",
        first_load_visible_time);
  }
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "ContentSettings.Popups.EngagementTime", total_foreground_duration,
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromHours(6), 50);
  if (web_contents()->GetClosedByUserGesture()) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "ContentSettings.Popups.EngagementTime.GestureClose",
        total_foreground_duration, base::TimeDelta::FromMilliseconds(1),
        base::TimeDelta::FromHours(6), 50);
  }

  if (opener_source_id_ != ukm::kInvalidSourceId) {
    const int kMaxInteractions = 100;
    int capped_interactions = num_interactions_ > kMaxInteractions
                                  ? kMaxInteractions
                                  : num_interactions_;
    ukm::builders::Popup_Closed(opener_source_id_)
        .SetEngagementTime(ukm::GetExponentialBucketMinForUserTiming(
            total_foreground_duration.InMilliseconds()))
        .SetUserInitiatedClose(web_contents()->GetClosedByUserGesture())
        .SetTrusted(is_trusted_)
        .SetNumInteractions(capped_interactions)
        .Record(ukm::UkmRecorder::Get());
  }
}

void PopupTracker::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (!first_load_visible_time_start_) {
    first_load_visible_time_start_ =
        visibility_tracker_.GetForegroundDuration();
  } else if (!first_load_visible_time_) {
    first_load_visible_time_ = visibility_tracker_.GetForegroundDuration() -
                               *first_load_visible_time_start_;
  }
}

void PopupTracker::OnVisibilityChanged(content::Visibility visibility) {
  // TODO(csharrison): Consider handling OCCLUDED tabs the same way as HIDDEN
  // tabs.
  if (visibility == content::Visibility::HIDDEN)
    visibility_tracker_.OnHidden();
  else
    visibility_tracker_.OnShown();
}

void PopupTracker::DidGetUserInteraction(
    const blink::WebInputEvent::Type type) {
  // TODO(csharrison): It would be nice if ctrl-W could be filtered out here,
  // but the initial ctrl key press is registered as a kRawKeyDown.
  num_interactions_++;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PopupTracker)
