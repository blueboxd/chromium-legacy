// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_ANDROID_METRICS_WEBLAYER_METRICS_SERVICE_ACCESSOR_H_
#define WEBLAYER_BROWSER_ANDROID_METRICS_WEBLAYER_METRICS_SERVICE_ACCESSOR_H_

#include <stdint.h>
#include <vector>

#include "components/metrics/metrics_service_accessor.h"

namespace weblayer {

// This class limits and documents access to metrics service helper methods.
// Since these methods are private, each user has to be explicitly declared
// as a 'friend' below.
class WebLayerMetricsServiceAccessor : public metrics::MetricsServiceAccessor {
 private:
  // For RegisterSyntheticMultiGroupFieldTrial.
  friend class WebLayerMetricsServiceClient;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebLayerMetricsServiceAccessor);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_ANDROID_METRICS_WEBLAYER_METRICS_SERVICE_ACCESSOR_H_