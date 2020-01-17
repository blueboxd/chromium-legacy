// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_FEATURES_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"

namespace features {

extern const base::Feature kCustomizedTabLoadTimeout;
extern const base::Feature kTabFreeze;
extern const base::Feature kStaggeredBackgroundTabOpening;
extern const base::Feature kStaggeredBackgroundTabOpeningExperiment;
extern const base::Feature kTabRanker;

}  // namespace features

namespace resource_coordinator {

// The name of the TabFreeze feature.
extern const char kTabFreezeFeatureName[];

// The name of the |ShouldPeriodicallyUnfreeze| parameter of the
// TabFreeze feature.
extern const char kTabFreeze_ShouldPeriodicallyUnfreezeParam[];

// The name of the |FreezingProtectMediaOnly| parameter of the
// TabFreeze feature.
extern const char kTabFreeze_FreezingProtectMediaOnlyParam[];

// Parameters used by the tab freezing feature.
struct TabFreezeParams {
  TabFreezeParams();
  TabFreezeParams(const TabFreezeParams& rhs);

  // Static definition of the different parameters that can be used by this
  // feature.

  static constexpr base::FeatureParam<bool> kShouldPeriodicallyUnfreeze{
      &features::kTabFreeze, kTabFreeze_ShouldPeriodicallyUnfreezeParam, false};
  static constexpr base::FeatureParam<int> kFreezeTimeout{
      &features::kTabFreeze, "FreezeTimeout",
      5 * base::Time::kSecondsPerMinute};
  static constexpr base::FeatureParam<int> kUnfreezeTimeout{
      &features::kTabFreeze, "UnfreezeTimeout",
      15 * base::Time::kSecondsPerMinute};
  static constexpr base::FeatureParam<int> kRefreezeTimeout{
      &features::kTabFreeze, "RefreezeTimeout", 10};

  static constexpr base::FeatureParam<bool> kFreezingProtectMediaOnly{
      &features::kTabFreeze, kTabFreeze_FreezingProtectMediaOnlyParam, false};

  // Whether frozen tabs should periodically be unfrozen to update their state.
  bool should_periodically_unfreeze;
  // Amount of time a tab must be occluded before it is frozen.
  base::TimeDelta freeze_timeout;
  // Amount of time a tab must be unfrozen before it is temporarily unfrozen.
  base::TimeDelta unfreeze_timeout;
  // Amount of time that a tab stays unfrozen before being frozen again.
  base::TimeDelta refreeze_timeout;
  // Only media tabs are protected from freezing.
  bool freezing_protect_media_only;
};

// Gets parameters for the tab freezing feature. This does no parameter
// validation, and sets the default values if the feature is not enabled.
TabFreezeParams GetTabFreezeParams();

// Return a static TabFreezeParams object that can be used by all the classes
// that need one.
const TabFreezeParams& GetStaticTabFreezeParams();
TabFreezeParams* GetMutableStaticTabFreezeParamsForTesting();

base::TimeDelta GetTabLoadTimeout(const base::TimeDelta& default_timeout);

// Gets number of oldest tab that should be scored by TabRanker.
int GetNumOldestTabsToScoreWithTabRanker();

// Gets ProcessType of tabs that should be scored by TabRanker.
int GetProcessTypeToScoreWithTabRanker();

// Gets number of oldest tabs that should be logged by TabRanker.
int GetNumOldestTabsToLogWithTabRanker();

// Whether to disable background time TabMetrics log.
bool DisableBackgroundLogWithTabRanker();

// Gets reload count penalty parameter for TabRanker.
float GetDiscardCountPenaltyTabRanker();

// Gets mru penalty parameter that converts mru index to scores.
float GetMRUScorerPenaltyTabRanker();

// Gets which type of scorer to use for TabRanker.
int GetScorerTypeForTabRanker();

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_FEATURES_H_
