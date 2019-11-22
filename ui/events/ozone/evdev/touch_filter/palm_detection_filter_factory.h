// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_PALM_DETECTION_FILTER_FACTORY_H_
#define UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_PALM_DETECTION_FILTER_FACTORY_H_

#include <memory>
#include <set>
#include <string>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/shared_palm_detection_filter_state.h"

namespace ui {

COMPONENT_EXPORT(EVDEV)
std::unique_ptr<PalmDetectionFilter> CreatePalmDetectionFilter(
    const EventDeviceInfo& devinfo,
    SharedPalmDetectionFilterState* shared_palm_state);

namespace internal {
// In a named namespace for testing.

COMPONENT_EXPORT(EVDEV)
std::vector<float> ParseRadiusPolynomial(const std::string& radius_string);
}  // namespace internal

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_PALM_DETECTION_FILTER_FACTORY_H_
