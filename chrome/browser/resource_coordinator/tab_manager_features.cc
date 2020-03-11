// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager_features.h"

#include "base/metrics/field_trial_params.h"
#include "base/numerics/ranges.h"
#include "chrome/common/chrome_features.h"

namespace {

constexpr char kTabLoadTimeoutInMsParameterName[] = "tabLoadTimeoutInMs";

}  // namespace

namespace features {

// Enables using customized value for tab load timeout. This is used by both
// staggered background tab opening and session restore in finch experiment to
// see what timeout value is better. The default timeout is used when this
// feature is disabled.
const base::Feature kCustomizedTabLoadTimeout{
    "CustomizedTabLoadTimeout", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables tab freezing.
const base::Feature kTabFreeze{resource_coordinator::kTabFreezeFeatureName,
                               base::FEATURE_DISABLED_BY_DEFAULT};

// Enables delaying the navigation of background tabs in order to improve
// foreground tab's user experience.
const base::Feature kStaggeredBackgroundTabOpening{
    "StaggeredBackgroundTabOpening", base::FEATURE_DISABLED_BY_DEFAULT};

// This controls whether we are running experiment with staggered background
// tab opening feature. For control group, this should be disabled. This depends
// on |kStaggeredBackgroundTabOpening| above.
const base::Feature kStaggeredBackgroundTabOpeningExperiment{
    "StaggeredBackgroundTabOpeningExperiment",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables using the Tab Ranker to score tabs for discarding instead of relying
// on last focused time.
const base::Feature kTabRanker{"TabRanker", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features

namespace resource_coordinator {

const char kTabFreezeFeatureName[] = "TabFreeze";
const char kTabFreeze_ShouldPeriodicallyUnfreezeParam[] =
    "ShouldPeriodicallyUnfreeze";
const char kTabFreeze_FreezingProtectMediaOnlyParam[] =
    "FreezingProtectMediaOnly";

constexpr base::FeatureParam<bool> TabFreezeParams::kShouldPeriodicallyUnfreeze;
constexpr base::FeatureParam<int> TabFreezeParams::kFreezeTimeout;
constexpr base::FeatureParam<int> TabFreezeParams::kUnfreezeTimeout;
constexpr base::FeatureParam<int> TabFreezeParams::kRefreezeTimeout;
constexpr base::FeatureParam<bool> TabFreezeParams::kFreezingProtectMediaOnly;

TabFreezeParams::TabFreezeParams() = default;
TabFreezeParams::TabFreezeParams(const TabFreezeParams& rhs) = default;

TabFreezeParams GetTabFreezeParams() {
  // TimeDelta::Max() should be used to express infinite timeouts. A large
  // timeout that is not TimeDelta::Max() causes MessageLoop to output a
  // warning.
  constexpr base::TimeDelta kLargeTimeout = base::TimeDelta::FromDays(14);

  TabFreezeParams params = {};

  params.should_periodically_unfreeze =
      TabFreezeParams::kShouldPeriodicallyUnfreeze.Get();

  params.freeze_timeout =
      base::TimeDelta::FromSeconds(TabFreezeParams::kFreezeTimeout.Get());
  DCHECK_LT(params.freeze_timeout, kLargeTimeout);

  params.unfreeze_timeout =
      base::TimeDelta::FromSeconds(TabFreezeParams::kUnfreezeTimeout.Get());
  DCHECK_LT(params.unfreeze_timeout, kLargeTimeout);

  params.refreeze_timeout =
      base::TimeDelta::FromSeconds(TabFreezeParams::kRefreezeTimeout.Get());
  DCHECK_LT(params.refreeze_timeout, kLargeTimeout);

  params.freezing_protect_media_only =
      TabFreezeParams::kFreezingProtectMediaOnly.Get();

  return params;
}

const TabFreezeParams& GetStaticTabFreezeParams() {
  static base::NoDestructor<TabFreezeParams> params(GetTabFreezeParams());
  return *params;
}

TabFreezeParams* GetMutableStaticTabFreezeParamsForTesting() {
  return const_cast<TabFreezeParams*>(&GetStaticTabFreezeParams());
}

base::TimeDelta GetTabLoadTimeout(const base::TimeDelta& default_timeout) {
  int timeout_in_ms = base::GetFieldTrialParamByFeatureAsInt(
      features::kCustomizedTabLoadTimeout, kTabLoadTimeoutInMsParameterName,
      default_timeout.InMilliseconds());

  if (timeout_in_ms <= 0)
    return default_timeout;

  return base::TimeDelta::FromMilliseconds(timeout_in_ms);
}

int GetNumOldestTabsToScoreWithTabRanker() {
  return base::GetFieldTrialParamByFeatureAsInt(
      features::kTabRanker, "number_of_oldest_tabs_to_score_with_TabRanker",
      50);
}

int GetProcessTypeToScoreWithTabRanker() {
  return base::GetFieldTrialParamByFeatureAsInt(
      features::kTabRanker, "process_type_of_tabs_to_score_with_TabRanker", 3);
}

int GetNumOldestTabsToLogWithTabRanker() {
  return base::GetFieldTrialParamByFeatureAsInt(
      features::kTabRanker, "number_of_oldest_tabs_to_log_with_TabRanker", 0);
}

bool DisableBackgroundLogWithTabRanker() {
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kTabRanker, "disable_background_log_with_TabRanker", true);
}

float GetDiscardCountPenaltyTabRanker() {
  return static_cast<float>(base::GetFieldTrialParamByFeatureAsDouble(
      features::kTabRanker, "discard_count_penalty", 0.0));
}

float GetMRUScorerPenaltyTabRanker() {
  return static_cast<float>(base::GetFieldTrialParamByFeatureAsDouble(
      features::kTabRanker, "mru_scorer_penalty", 1.0));
}

int GetScorerTypeForTabRanker() {
  return base::GetFieldTrialParamByFeatureAsInt(features::kTabRanker,
                                                "scorer_type", 1);
}

}  // namespace resource_coordinator
