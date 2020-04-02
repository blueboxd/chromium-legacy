// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/contextual_nudge_status_tracker.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"

namespace {

// Tracks the amount of time between showing the user a nudge and the user
// performing the gesture.
constexpr char time_delta_histogram_suffix[] = ".GestureTimeDelta";
// Tracks how the user exits the state for which the contextual nudge was shown.
constexpr char hide_nudge_method_histogram_suffix[] = ".DismissNudgeReason";

// The maximum number of seconds that should be recorded in the TimeDelta
// histogram. Time between showing the nudge and recording the gesture are
// separated into 61 buckets: 0-1 second, 1-2 second ... 59-60 seconds and 60+
// seconds.
constexpr int kMaxHistogramTime = 61;

std::string GetTimeDeltaHistogramName(const std::string& histogram_prefix) {
  return histogram_prefix + time_delta_histogram_suffix;
}

std::string GetEnumHistogramName(const std::string& histogram_prefix) {
  return histogram_prefix + hide_nudge_method_histogram_suffix;
}

std::string GetMetricPrefix(ash::contextual_tooltip::TooltipType type) {
  switch (type) {
    case ash::contextual_tooltip::TooltipType::kInAppToHome:
      return "Ash.ContextualNudge.InAppToHome";
    case ash::contextual_tooltip::TooltipType::kBackGesture:
      return "Ash.ContextualNudge.BackGesture";
    case ash::contextual_tooltip::TooltipType::kHomeToOverview:
      return "Ash.ContextualNudge.HomeToOverview";
  }
}

}  // namespace

namespace ash {

ContextualNudgeStatusTracker::ContextualNudgeStatusTracker(
    ash::contextual_tooltip::TooltipType type)
    : type_(type) {}

ContextualNudgeStatusTracker::~ContextualNudgeStatusTracker() = default;

void ContextualNudgeStatusTracker::HandleNudgeShown(
    base::TimeTicks shown_time) {
  nudge_shown_time_ = shown_time;
  has_nudge_been_shown_ = true;
  visible_ = true;
}

void ContextualNudgeStatusTracker::HandleGesturePerformed(
    base::TimeTicks hide_time) {
  if (visible_) {
    LogNudgeDismissedMetrics(
        contextual_tooltip::DismissNudgeReason::kPerformedGesture);
  }

  if (!has_nudge_been_shown_)
    return;
  base::TimeDelta time_since_show = hide_time - nudge_shown_time_;
  base::UmaHistogramCustomTimes(
      GetTimeDeltaHistogramName(GetMetricPrefix(type_)), time_since_show,
      base::TimeDelta::FromSeconds(1),
      base::TimeDelta::FromSeconds(kMaxHistogramTime), kMaxHistogramTime);
  has_nudge_been_shown_ = false;
}

void ContextualNudgeStatusTracker::LogNudgeDismissedMetrics(
    contextual_tooltip::DismissNudgeReason reason) {
  if (!visible_ || !has_nudge_been_shown_)
    return;
  base::UmaHistogramEnumeration(GetEnumHistogramName(GetMetricPrefix(type_)),
                                reason);
  visible_ = false;
}

}  // namespace ash
