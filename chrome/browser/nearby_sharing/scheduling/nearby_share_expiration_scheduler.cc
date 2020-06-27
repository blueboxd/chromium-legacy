// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/scheduling/nearby_share_expiration_scheduler.h"

#include <algorithm>
#include <utility>

NearbyShareExpirationScheduler::NearbyShareExpirationScheduler(
    ExpirationTimeCallback expiration_time_callback,
    bool retry_failures,
    bool require_connectivity,
    const std::string& pref_name,
    PrefService* pref_service,
    OnRequestCallback on_request_callback,
    const base::Clock* clock)
    : NearbyShareSchedulerBase(retry_failures,
                               require_connectivity,
                               pref_name,
                               pref_service,
                               std::move(on_request_callback),
                               clock),
      expiration_time_callback_(std::move(expiration_time_callback)) {}

NearbyShareExpirationScheduler::~NearbyShareExpirationScheduler() = default;

base::Optional<base::TimeDelta>
NearbyShareExpirationScheduler::TimeUntilRecurringRequest(
    base::Time now) const {
  return std::max(base::TimeDelta::FromSeconds(0),
                  expiration_time_callback_.Run() - now);
}
