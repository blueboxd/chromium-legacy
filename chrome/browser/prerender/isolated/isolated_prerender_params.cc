// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_params.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_features.h"
#include "chrome/common/chrome_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"

bool IsolatedPrerenderIsEnabled() {
  return base::FeatureList::IsEnabled(features::kIsolatePrerenders);
}

base::Optional<size_t> IsolatedPrerenderMaximumNumberOfPrefetches() {
  if (!IsolatedPrerenderIsEnabled()) {
    return 0;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          "isolated-prerender-unlimited-prefetches")) {
    return base::nullopt;
  }

  int max = base::GetFieldTrialParamByFeatureAsInt(features::kIsolatePrerenders,
                                                   "max_srp_prefetches", 1);
  if (max < 0) {
    return base::nullopt;
  }
  return max;
}

base::TimeDelta IsolatedPrerenderProbeTimeout() {
  return base::TimeDelta::FromMilliseconds(
      base::GetFieldTrialParamByFeatureAsInt(
          features::kIsolatePrerendersMustProbeOrigin, "probe_timeout_ms",
          10 * 1000 /* 10 seconds */));
}

bool IsolatedPrerenderCloseIdleSockets() {
  return base::GetFieldTrialParamByFeatureAsBool(features::kIsolatePrerenders,
                                                 "close_idle_sockets", true);
}

base::TimeDelta IsolatedPrefetchTimeoutDuration() {
  return base::TimeDelta::FromMilliseconds(
      base::GetFieldTrialParamByFeatureAsInt(features::kIsolatePrerenders,
                                             "prefetch_timeout_ms",
                                             10 * 1000 /* 10 seconds */));
}
