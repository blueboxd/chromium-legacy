// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DEMOGRAPHIC_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_DEMOGRAPHIC_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

namespace base {
struct Feature;
}

namespace metrics {

class DemographicMetricsProvider : public MetricsProvider {
 public:
  DemographicMetricsProvider() = default;
  ~DemographicMetricsProvider() override = default;

  // Kill switch to stop reporting demographic data.
  static const base::Feature kDemographicMetricsReporting;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_DEMOGRAPHIC_METRICS_PROVIDER_H_