// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/demographic_metrics_provider.h"

#include "base/feature_list.h"
#include "base/optional.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/base/user_demographics.h"
#include "components/sync/driver/sync_service.h"

namespace metrics {

// static
const base::Feature DemographicMetricsProvider::kDemographicMetricsReporting = {
    "DemographicMetricsReporting", base::FEATURE_DISABLED_BY_DEFAULT};

DemographicMetricsProvider::DemographicMetricsProvider(
    std::unique_ptr<ProfileClient> profile_client)
    : profile_client_(std::move(profile_client)) {
  DCHECK(profile_client_);
}

DemographicMetricsProvider::~DemographicMetricsProvider() {}

void DemographicMetricsProvider::ProvideCurrentSessionData(
    ChromeUserMetricsExtension* uma_proto) {
  // Skip if feature disabled.
  if (!base::FeatureList::IsEnabled(kDemographicMetricsReporting))
    return;

  // Skip if not exactly one Profile on disk. Having more than one Profile that
  // is using the browser can make demographics less relevant. This approach
  // cannot determine if there is more than 1 distinct user using the Profile.
  if (profile_client_->GetNumberOfProfilesOnDisk() != 1) {
    syncer::LogUserDemographicsStatus(
        syncer::UserDemographicsStatus::kMoreThanOneProfile);
    return;
  }

  syncer::SyncService* sync_service = profile_client_->GetSyncService();
  // Skip if no sync service.
  if (!sync_service) {
    syncer::LogUserDemographicsStatus(
        syncer::UserDemographicsStatus::kNoSyncService);
    return;
  }

  syncer::UserDemographicsResult demographics_result =
      sync_service->GetUserDemographics(profile_client_->GetNetworkTime());
  syncer::LogUserDemographicsStatus(demographics_result.status());
  // Report demographics when they can be provided.
  if (demographics_result.IsSuccess()) {
    auto* demographics_proto = uma_proto->mutable_user_demographics();
    demographics_proto->set_birth_year(demographics_result.value().birth_year);
    demographics_proto->set_gender(demographics_result.value().gender);
  }
}

}  // namespace metrics
