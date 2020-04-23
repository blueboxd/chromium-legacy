// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_onboarding.h"

#include "base/feature_list.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_manager {


OnboardingStateUpdate::OnboardingStateUpdate(
    scoped_refptr<password_manager::PasswordStore> store,
    PrefService* prefs)
    : store_(std::move(store)), prefs_(prefs) {}

void OnboardingStateUpdate::Start() {
  store_->GetAutofillableLogins(this);
}

OnboardingStateUpdate::~OnboardingStateUpdate() = default;

void OnboardingStateUpdate::UpdateState(
    std::vector<std::unique_ptr<autofill::PasswordForm>> credentials) {
  if (credentials.size() >= kOnboardingCredentialsThreshold) {
    if (prefs_->GetInteger(prefs::kPasswordManagerOnboardingState) ==
        static_cast<int>(OnboardingState::kShouldShow)) {
      prefs_->SetInteger(prefs::kPasswordManagerOnboardingState,
                         static_cast<int>(OnboardingState::kDoNotShow));
    }
    return;
  }
  if (prefs_->GetInteger(
          password_manager::prefs::kPasswordManagerOnboardingState) ==
      static_cast<int>(OnboardingState::kDoNotShow)) {
    prefs_->SetInteger(prefs::kPasswordManagerOnboardingState,
                       static_cast<int>(OnboardingState::kShouldShow));
  }
}

void OnboardingStateUpdate::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  UpdateState(std::move(results));
  delete this;
}

// Initializes and runs the OnboardingStateUpdate class, which is
// used to update the |kPasswordManagerOnboardingState| pref.
void StartOnboardingStateUpdate(
    scoped_refptr<password_manager::PasswordStore> store,
    PrefService* prefs) {
  (new OnboardingStateUpdate(store, prefs))->Start();
}

void UpdateOnboardingState(scoped_refptr<password_manager::PasswordStore> store,
                           PrefService* prefs,
                           base::TimeDelta delay) {
  if (prefs->GetInteger(prefs::kPasswordManagerOnboardingState) ==
      static_cast<int>(OnboardingState::kShown)) {
    return;
  }
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&StartOnboardingStateUpdate, store, prefs),
      delay);
}

bool ShouldShowOnboarding(PrefService* prefs,
                          PasswordUpdateBool is_password_update,
                          BlacklistedBool is_blacklisted,
                          SyncState sync_state) {
  if (is_blacklisted) {
    return false;
  }

  if (is_password_update) {
    return false;
  }

  bool was_feature_checked_before = prefs->GetBoolean(
      password_manager::prefs::kWasOnboardingFeatureCheckedBefore);

  if (was_feature_checked_before) {
    // This is a signal that the user was at some point eligible for onboarding.
    // The feature needs to be checked again, irrespective of onboarding status,
    // in order to ensure data completeness.
    ignore_result(base::FeatureList::IsEnabled(
        password_manager::features::kPasswordManagerOnboardingAndroid));
  }

  if (sync_state == NOT_SYNCING) {
    return false;
  }

  int pref_value = prefs->GetInteger(
      password_manager::prefs::kPasswordManagerOnboardingState);
  bool should_show =
      (pref_value == static_cast<int>(OnboardingState::kShouldShow));
  if (!should_show)
    return false;

  // It is very important that the feature is checked only for users who
  // are or were eligible for onboarding, otherwise the data will be diluted.
  // It's also important that the feature is checked in all eligible cases,
  // including past eligibiliy and users having already seen the onboarding
  // prompt, otherwise the data will be incomplete.
  prefs->SetBoolean(password_manager::prefs::kWasOnboardingFeatureCheckedBefore,
                    true);
  return base::FeatureList::IsEnabled(
      password_manager::features::kPasswordManagerOnboardingAndroid);
}

}  // namespace password_manager
