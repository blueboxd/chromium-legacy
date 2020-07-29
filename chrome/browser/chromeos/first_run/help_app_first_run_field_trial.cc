// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/first_run/help_app_first_run_field_trial.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/channel_info.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"

namespace help_app_first_run_field_trial {
namespace {

// String local state preference with the name of the assigned trial group.
// Empty if no group has been assigned yet.
const char kTrialGroupPrefName[] = "help_app_first_run.trial_group";

// The field trial name.
const char kTrialName[] = "HelpAppFirstRun";

// Group names for the trial.
const char kDisabledGroup[] = "Disabled";
const char kDefaultGroup[] = "Default";

// Probabilities for all field trial groups add up to kTotalProbability.
const base::FieldTrial::Probability kTotalProbability = 100;

// Creates the field trial.
scoped_refptr<base::FieldTrial> CreateFieldTrial() {
  return base::FieldTrialList::FactoryGetFieldTrial(
      kTrialName, kTotalProbability, kDefaultGroup,
      base::FieldTrial::ONE_TIME_RANDOMIZED,
      /*default_group_number=*/nullptr);
}

// Sets the feature state based on the trial group. Defaults to enabled.
void SetFeatureState(base::FeatureList* feature_list,
                     base::FieldTrial* trial,
                     const std::string& group_name) {
  base::FeatureList::OverrideState feature_state =
      group_name == kDisabledGroup ? base::FeatureList::OVERRIDE_DISABLE_FEATURE
                                   : base::FeatureList::OVERRIDE_ENABLE_FEATURE;
  feature_list->RegisterFieldTrialOverride(
      chromeos::features::kHelpAppFirstRun.name, feature_state, trial);
}

// Creates a trial with a single group and sets the feature flag to the state
// for that group.
void CreateSubsequentRunTrial(base::FeatureList* feature_list,
                              const std::string& group_name) {
  scoped_refptr<base::FieldTrial> trial = CreateFieldTrial();
  trial->AppendGroup(group_name, kTotalProbability);
  SetFeatureState(feature_list, trial.get(), group_name);
}

}  // namespace

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kTrialGroupPrefName, std::string());
}

void Create(base::FeatureList* feature_list, PrefService* local_state) {
  std::string trial_group = local_state->GetString(kTrialGroupPrefName);
  // We don't want to add new users to the trial anymore.
  if (!trial_group.empty()) {
    // Group already assigned.
    CreateSubsequentRunTrial(feature_list, trial_group);
  }
}

}  // namespace help_app_first_run_field_trial
