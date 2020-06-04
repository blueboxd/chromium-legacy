// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/features.h"

namespace blink {
namespace scheduler {

const base::Feature kIntensiveWakeUpThrottling{
    "IntensiveWakeUpThrottling", base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<int>
    kIntensiveWakeUpThrottling_DurationBetweenWakeUpsSeconds{
        &kIntensiveWakeUpThrottling, "duration_between_wake_ups_seconds", 60};
const base::FeatureParam<int> kIntensiveWakeUpThrottling_GracePeriodSeconds{
    &kIntensiveWakeUpThrottling, "grace_period_seconds", 5 * 60};

}  // namespace scheduler
}  // namespace blink
