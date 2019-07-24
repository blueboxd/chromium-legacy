// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_background_sync_controller.h"

namespace content {

MockBackgroundSyncController::MockBackgroundSyncController() = default;
MockBackgroundSyncController::~MockBackgroundSyncController() = default;

void MockBackgroundSyncController::NotifyOneShotBackgroundSyncRegistered(
    const url::Origin& origin,
    bool can_fire,
    bool is_reregistered) {
  registration_count_ += 1;
  registration_origin_ = origin;
}

void MockBackgroundSyncController::ScheduleBrowserWakeUp(
    blink::mojom::BackgroundSyncType sync_type) {
  if (sync_type == blink::mojom::BackgroundSyncType::PERIODIC) {
    run_in_background_for_periodic_sync_count_ += 1;
    return;
  }
  run_in_background_for_one_shot_sync_count_ += 1;
}

void MockBackgroundSyncController::GetParameterOverrides(
    BackgroundSyncParameters* parameters) {
  *parameters = background_sync_parameters_;
}

base::TimeDelta MockBackgroundSyncController::GetNextEventDelay(
    const BackgroundSyncRegistration& registration,
    BackgroundSyncParameters* parameters) {
  DCHECK(parameters);

  if (suspended_periodic_sync_origins_.count(registration.origin()))
    return base::TimeDelta::Max();

  int num_attempts = registration.num_attempts();

  if (!num_attempts) {
    // First attempt.
    switch (registration.sync_type()) {
      case blink::mojom::BackgroundSyncType::ONE_SHOT:
        return base::TimeDelta();
      case blink::mojom::BackgroundSyncType::PERIODIC:
        int64_t effective_gap_ms =
            parameters->min_periodic_sync_events_interval.InMilliseconds();
        return base::TimeDelta::FromMilliseconds(
            std::max(registration.options()->min_interval, effective_gap_ms));
    }
  }

  // After a sync event has been fired.
  DCHECK_LT(num_attempts, parameters->max_sync_attempts);
  return parameters->initial_retry_delay *
         pow(parameters->retry_delay_factor, num_attempts - 1);
}

std::unique_ptr<BackgroundSyncController::BackgroundSyncEventKeepAlive>
MockBackgroundSyncController::CreateBackgroundSyncEventKeepAlive() {
  return nullptr;
}

void MockBackgroundSyncController::NoteSuspendedPeriodicSyncOrigins(
    std::set<url::Origin> suspended_origins) {
  for (auto& origin : suspended_origins) {
    suspended_periodic_sync_origins_.insert(std::move(origin));
  }
}

}  // namespace content
