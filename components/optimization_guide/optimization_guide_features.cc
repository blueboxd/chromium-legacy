// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/optimization_guide_features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "components/optimization_guide/optimization_guide_constants.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_switches.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"

namespace optimization_guide {
namespace features {

// Enables the syncing of the Optimization Hints component, which provides
// hints for what Previews can be applied on a page load.
const base::Feature kOptimizationHints {
  "OptimizationHints",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else   // !defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#endif  // defined(OS_ANDROID)
};

// Enables Optimization Hints that are marked as experimental. Optimizations are
// marked experimental by setting an experiment name in the "experiment_name"
// field of the Optimization proto. This allows experiments at the granularity
// of a single PreviewType for a single host (or host suffix). The intent is
// that optimizations that may not work properly for certain sites can be tried
// at a small scale via Finch experiments. Experimental optimizations can be
// activated by enabling this feature and passing an experiment name as a
// parameter called "experiment_name" that matches the experiment name in the
// Optimization proto.
const base::Feature kOptimizationHintsExperiments{
    "OptimizationHintsExperiments", base::FEATURE_DISABLED_BY_DEFAULT};

// Provides slow page triggering parameters.
const base::Feature kSlowPageTriggering{"PreviewsSlowPageTriggering",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables fetching optimization hints from a remote Optimization Guide Service.
const base::Feature kOptimizationHintsFetching{
    "OptimizationHintsFetching", base::FEATURE_DISABLED_BY_DEFAULT};

size_t MaxHintsFetcherTopHostBlacklistSize() {
  // The blacklist will be limited to the most engaged hosts and will hold twice
  // (2*N) as many hosts that the HintsFetcher request hints for. The extra N
  // hosts on the blacklist are meant to cover the case that the engagement
  // scores on some of the top N host engagement scores decay and they fall out
  // of the top N.
  return 2 * MaxHostsForOptimizationGuideServiceHintsFetch();
}

size_t MaxHostsForOptimizationGuideServiceHintsFetch() {
  return GetFieldTrialParamByFeatureAsInt(
      features::kOptimizationHintsFetching,
      "max_hosts_for_optimization_guide_service_hints_fetch", 30);
}

base::TimeDelta StoredFetchedHintsFreshnessDuration() {
  return base::TimeDelta::FromDays(GetFieldTrialParamByFeatureAsInt(
      features::kOptimizationHintsFetching,
      "max_store_duration_for_featured_hints_in_days", 7));
}

std::string GetOptimizationGuideServiceAPIKey() {
  // Command line override takes priority.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kOptimizationGuideServiceAPIKey)) {
    return command_line->GetSwitchValueASCII(
        switches::kOptimizationGuideServiceAPIKey);
  }

  return google_apis::GetAPIKey();
}

GURL GetOptimizationGuideServiceURL() {
  // Command line override takes priority.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kOptimizationGuideServiceURL)) {
    // Assume the command line switch is correct and return it.
    return GURL(command_line->GetSwitchValueASCII(
        switches::kOptimizationGuideServiceURL));
  }

  std::string url = base::GetFieldTrialParamValueByFeature(
      features::kOptimizationHintsFetching, "optimization_guide_service_url");
  if (url.empty() || !GURL(url).SchemeIs(url::kHttpsScheme)) {
    if (!url.empty())
      LOG(WARNING)
          << "Empty or invalid optimization_guide_service_url provided: "
          << url;
    return GURL(kOptimizationGuideServiceDefaultURL);
  }

  return GURL(url);
}

bool IsOptimizationHintsEnabled() {
  return base::FeatureList::IsEnabled(features::kOptimizationHints);
}

bool IsHintsFetchingEnabled() {
  return base::FeatureList::IsEnabled(features::kOptimizationHintsFetching);
}

}  // namespace features
}  // namespace optimization_guide
