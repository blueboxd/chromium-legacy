// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_PALM_DETECTION_FILTER_FACTORY_H_
#define UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_PALM_DETECTION_FILTER_FACTORY_H_

#include <memory>
#include <set>
#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"
#include "ui/events/ozone/evdev/touch_filter/heuristic_stylus_palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/open_palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/shared_palm_detection_filter_state.h"
namespace ui {

extern const EVENTS_OZONE_EVDEV_EXPORT base::Feature
    kEnableHeuristicPalmDetectionFilter;

extern const EVENTS_OZONE_EVDEV_EXPORT base::FeatureParam<double>
    kHeuristicCancelThresholdSeconds;
extern const EVENTS_OZONE_EVDEV_EXPORT base::FeatureParam<double>
    kHeuristicHoldThresholdSeconds;
extern const EVENTS_OZONE_EVDEV_EXPORT base::FeatureParam<int>
    kHeuristicStrokeCount;
std::unique_ptr<PalmDetectionFilter> EVENTS_OZONE_EVDEV_EXPORT
CreatePalmDetectionFilter(const EventDeviceInfo& devinfo,
                          SharedPalmDetectionFilterState* shared_palm_state);
}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_PALM_DETECTION_FILTER_FACTORY_H_
