// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_FEATURES_H_
#define UI_EVENTS_OZONE_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "ui/events/ozone/events_ozone_export.h"

namespace ui {

EVENTS_OZONE_EXPORT
extern const base::Feature kEnableHeuristicPalmDetectionFilter;

EVENTS_OZONE_EXPORT
extern const base::Feature kEnableNeuralPalmDetectionFilter;

EVENTS_OZONE_EXPORT
extern const base::FeatureParam<std::string> kNeuralPalmRadiusPolynomial;

EVENTS_OZONE_EXPORT
extern const base::FeatureParam<double> kHeuristicCancelThresholdSeconds;

EVENTS_OZONE_EXPORT
extern const base::FeatureParam<double> kHeuristicHoldThresholdSeconds;

EVENTS_OZONE_EXPORT
extern const base::FeatureParam<int> kHeuristicStrokeCount;

}  // namespace ui

#endif  // UI_EVENTS_OZONE_FEATURES_H_
