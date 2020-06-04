// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/metrics_util.h"

#include "base/bind.h"

namespace ash {
namespace metrics_util {

namespace {

// Calculates smoothness from |throughput| and sends to |callback|.
void ForwardSmoothness(SmoothnessCallback callback,
                       cc::FrameSequenceMetrics::ThroughputData throughput) {
  const int smoothness = std::floor(100.0f * throughput.frames_produced /
                                    throughput.frames_expected);
  callback.Run(smoothness);
}

}  // namespace

ReportCallback ForSmoothness(SmoothnessCallback callback) {
  return base::BindRepeating(&ForwardSmoothness, std::move(callback));
}

}  // namespace metrics_util
}  // namespace ash
