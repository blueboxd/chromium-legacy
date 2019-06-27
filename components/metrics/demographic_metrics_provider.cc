// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/demographic_metrics_provider.h"
#include "base/feature_list.h"

namespace metrics {

// static
const base::Feature DemographicMetricsProvider::kDemographicMetricsReporting = {
    "DemographicMetricsReporting", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace metrics
