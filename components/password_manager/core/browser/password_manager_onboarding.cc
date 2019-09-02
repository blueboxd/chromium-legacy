// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_onboarding.h"

#include "base/feature_list.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_manager {

using OnboardingState = password_manager::metrics_util::OnboardingState;

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
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kPasswordManagerOnboardingAndroid)) {
    return;
  }
  if (prefs->GetInteger(prefs::kPasswordManagerOnboardingState) ==
      static_cast<int>(OnboardingState::kShown)) {
    return;
  }
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&StartOnboardingStateUpdate, store, prefs),
      delay);
}

bool ShouldShowOnboarding(PrefService* prefs, bool is_password_update) {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kPasswordManagerOnboardingAndroid)) {
    return false;
  }
  if (is_password_update) {
    return false;
  }
  return prefs->GetInteger(
             password_manager::prefs::kPasswordManagerOnboardingState) ==
         static_cast<int>(OnboardingState::kShouldShow);
}

}  // namespace password_manager
