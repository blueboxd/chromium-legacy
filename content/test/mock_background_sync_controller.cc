// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_background_sync_controller.h"

namespace content {

void MockBackgroundSyncController::NotifyBackgroundSyncRegistered(
    const url::Origin& origin,
    bool can_fire,
    bool is_reregistered) {
  registration_count_ += 1;
  registration_origin_ = origin;
}

void MockBackgroundSyncController::RunInBackground() {
  run_in_background_count_ += 1;
}

void MockBackgroundSyncController::GetParameterOverrides(
    BackgroundSyncParameters* parameters) const {
  *parameters = background_sync_parameters_;
}

// |origin| can be used to potentially suspend or penalize registrations based
// on the level of user engagement. That logic isn't tested here, and |origin|
// remains unused.
base::TimeDelta MockBackgroundSyncController::GetNextEventDelay(
    const url::Origin& origin,
    int64_t min_interval,
    int num_attempts,
    blink::mojom::BackgroundSyncType sync_type,
    BackgroundSyncParameters* parameters) const {
  DCHECK(parameters);

  if (!num_attempts) {
    // First attempt.
    switch (sync_type) {
      case blink::mojom::BackgroundSyncType::ONE_SHOT:
        return base::TimeDelta();
      case blink::mojom::BackgroundSyncType::PERIODIC:
        int64_t effective_gap_ms =
            parameters->min_periodic_sync_events_interval.InMilliseconds();
        return base::TimeDelta::FromMilliseconds(
            std::max(min_interval, effective_gap_ms));
    }
  }

  // After a sync event has been fired.
  DCHECK_LE(num_attempts, parameters->max_sync_attempts);
  return parameters->initial_retry_delay *
         pow(parameters->retry_delay_factor, num_attempts - 1);
}

std::unique_ptr<BackgroundSyncController::BackgroundSyncEventKeepAlive>
MockBackgroundSyncController::CreateBackgroundSyncEventKeepAlive() {
  return nullptr;
}

}  // namespace content
