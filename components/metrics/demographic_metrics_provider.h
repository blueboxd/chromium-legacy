// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DEMOGRAPHIC_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_DEMOGRAPHIC_METRICS_PROVIDER_H_

#include <memory>

#include "base/time/time.h"
#include "components/metrics/metrics_provider.h"
#include "components/sync/driver/sync_service.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace base {
struct Feature;
}

namespace metrics {

// Provider of user demographics. Aggregate, non-individually-identifying
// demographics are used to measure usage of Chrome features by age and gender.
// Users can avoid aggregation of usage data by demographics by either a)
// turning off sending usage statistics to Google or b) turning off sync.
class DemographicMetricsProvider : public MetricsProvider {
 public:
  // Interface that represents the client that retrieves Profile information.
  class ProfileClient {
   public:
    virtual ~ProfileClient() = default;

    // Gets the total number of profiles that are on disk (loaded + not loaded)
    // for the browser.
    virtual int GetNumberOfProfilesOnDisk() = 0;

    // Gets a weak pointer to the ProfileSyncService of the Profile.
    virtual syncer::SyncService* GetSyncService() = 0;

    // Gets the network time that represents now.
    virtual base::Time GetNetworkTime() const = 0;
  };

  explicit DemographicMetricsProvider(
      std::unique_ptr<ProfileClient> profile_client);
  ~DemographicMetricsProvider() override;

  // MetricsProvider:
  void ProvideCurrentSessionData(
      ChromeUserMetricsExtension* uma_proto) override;

  // Feature switch to report user demographic data.
  static const base::Feature kDemographicMetricsReporting;

 private:
  std::unique_ptr<ProfileClient> profile_client_;

  DISALLOW_COPY_AND_ASSIGN(DemographicMetricsProvider);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_DEMOGRAPHIC_METRICS_PROVIDER_H_