// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_ENTROPY_STATE_PROVIDER_H_
#define COMPONENTS_METRICS_ENTROPY_STATE_PROVIDER_H_

#include "components/metrics/entropy_state.h"
#include "components/metrics/metrics_provider.h"

class PrefService;

namespace metrics {

// EntropyStateProvider adds information about low entropy sources in the system
// profile. This includes |low_entropy_source| and |old_low_entropy_source|.
class EntropyStateProvider : public MetricsProvider {
 public:
  explicit EntropyStateProvider(PrefService* local_state);
  ~EntropyStateProvider() override;

  EntropyStateProvider(const EntropyStateProvider&) = delete;
  EntropyStateProvider& operator=(const EntropyStateProvider&) = delete;

  void ProvideSystemProfileMetrics(
      SystemProfileProto* system_profile_proto) override;

 private:
  EntropyState entropy_state_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_ENTROPY_STATE_PROVIDER_H_
