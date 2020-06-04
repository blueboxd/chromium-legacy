// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_METRICS_UTIL_H_
#define ASH_PUBLIC_CPP_METRICS_UTIL_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback.h"
#include "cc/metrics/frame_sequence_metrics.h"

namespace ash {
namespace metrics_util {

using ReportCallback =
    base::RepeatingCallback<void(cc::FrameSequenceMetrics::ThroughputData)>;
using SmoothnessCallback = base::RepeatingCallback<void(int smoothness)>;

// Returns a ReportCallback that could be passed to ui::ThroughputTracker
// or ui::AnimationThroughputReporter. The returned callback picks up the
// cc::FrameSequenceMetrics::ThroughputData, calculates the smoothness
// out of it and forward it to the smoothness report callback.
ASH_PUBLIC_EXPORT ReportCallback ForSmoothness(SmoothnessCallback callback);

}  // namespace metrics_util
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_METRICS_UTIL_H_
