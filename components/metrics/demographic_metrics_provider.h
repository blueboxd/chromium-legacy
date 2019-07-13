// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DEMOGRAPHIC_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_DEMOGRAPHIC_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"
#include "components/prefs/pref_service.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace base {
struct Feature;
}

namespace metrics {

class DemographicMetricsProvider : public MetricsProvider {
 public:
  // Constructs a demographic metrics provider that gets demographic data from
  // |pref_service|, which must outlive the use of an object of this class.
  explicit DemographicMetricsProvider(PrefService* pref_service);
  ~DemographicMetricsProvider() override = default;

  // MetricsProvider:
  void ProvideCurrentSessionData(
      ChromeUserMetricsExtension* uma_proto) override;

  // Feature switch for reporting demographic data.
  static const base::Feature kDemographicMetricsReporting;

 private:
  // Preference Service to get profile prefs from.
  PrefService* const pref_service_;

  DISALLOW_COPY_AND_ASSIGN(DemographicMetricsProvider);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_DEMOGRAPHIC_METRICS_PROVIDER_H_