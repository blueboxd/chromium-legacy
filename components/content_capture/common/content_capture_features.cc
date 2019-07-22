// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/common/content_capture_features.h"

#include "base/metrics/field_trial_params.h"

namespace content_capture {
namespace features {

const base::Feature kContentCapture{"ContentCapture",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

bool IsContentCaptureEnabled() {
  return base::FeatureList::IsEnabled(kContentCapture);
}

int TaskLongDelayInMilliseconds() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kContentCapture, "task_long_delay_in_milliseconds", 5000);
}

int TaskShortDelayInMilliseconds() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kContentCapture, "task_short_delay_in_milliseconds", 500);
}

}  // namespace features
}  // namespace content_capture
