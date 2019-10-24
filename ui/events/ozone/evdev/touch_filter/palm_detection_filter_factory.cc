// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/touch_filter/heuristic_stylus_palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/open_palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter.h"

namespace ui {

const base::Feature kEnableHeuristicPalmDetectionFilter{
    "EnableHeuristicPalmDetectionFilter", base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<double> kHeuristicCancelThresholdSeconds{
    &kEnableHeuristicPalmDetectionFilter,
    "heuristic_palm_cancel_threshold_seconds", 0.4};

const base::FeatureParam<double> kHeuristicHoldThresholdSeconds{
    &kEnableHeuristicPalmDetectionFilter,
    "heuristic_palm_hold_threshold_seconds", 1.0};

const base::FeatureParam<int> kHeuristicStrokeCount{
    &kEnableHeuristicPalmDetectionFilter, "heuristic_palm_stroke_count", 0};

std::unique_ptr<PalmDetectionFilter> CreatePalmDetectionFilter(
    const EventDeviceInfo& devinfo,
    SharedPalmDetectionFilterState* shared_palm_state) {
  if (base::FeatureList::IsEnabled(kEnableHeuristicPalmDetectionFilter)) {
    const base::TimeDelta hold_time =
        base::TimeDelta::FromSecondsD(kHeuristicHoldThresholdSeconds.Get());
    const base::TimeDelta cancel_time =
        base::TimeDelta::FromSecondsD(kHeuristicCancelThresholdSeconds.Get());
    const int stroke_count = kHeuristicStrokeCount.Get();
    return std::make_unique<HeuristicStylusPalmDetectionFilter>(
        shared_palm_state, stroke_count, hold_time, cancel_time);
  }
  return std::make_unique<OpenPalmDetectionFilter>(shared_palm_state);
}

}  // namespace ui
