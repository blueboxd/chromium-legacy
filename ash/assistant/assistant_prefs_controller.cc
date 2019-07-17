// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_prefs_controller.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash {

AssistantPrefsController::AssistantPrefsController()
    : session_observer_(this) {}

AssistantPrefsController::~AssistantPrefsController() = default;

void AssistantPrefsController::AddObserver(AssistantPrefsObserver* observer) {
  InitObserver(observer);
  observers_.AddObserver(observer);
}

void AssistantPrefsController::RemoveObserver(
    AssistantPrefsObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AssistantPrefsController::InitObserver(AssistantPrefsObserver* observer) {
  PrefService* primary_user_prefs =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  PrefService* active_prefs =
      Shell::Get()->session_controller()->GetActivePrefService();

  if (primary_user_prefs && primary_user_prefs == active_prefs) {
    observer->OnAssistantConsentStatusUpdated(active_prefs->GetInteger(
        chromeos::assistant::prefs::kAssistantConsentStatus));
  }
}

PrefService* AssistantPrefsController::prefs() {
  PrefService* primary_user_prefs =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  PrefService* active_prefs =
      Shell::Get()->session_controller()->GetActivePrefService();

  DCHECK_EQ(primary_user_prefs, active_prefs);
  return primary_user_prefs;
}

void AssistantPrefsController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  pref_change_registrar_.reset();

  // If primary user is active, register pref change listeners.
  PrefService* primary_user_prefs =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  if (primary_user_prefs && primary_user_prefs == pref_service) {
    for (auto& observer : observers_)
      InitObserver(&observer);

    pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pref_change_registrar_->Init(pref_service);

    pref_change_registrar_->Add(
        chromeos::assistant::prefs::kAssistantConsentStatus,
        base::BindRepeating(&AssistantPrefsController::NotifyConsentStatus,
                            base::Unretained(this)));
  }
}

void AssistantPrefsController::NotifyConsentStatus() {
  for (auto& observer : observers_) {
    observer.OnAssistantConsentStatusUpdated(
        Shell::Get()
            ->session_controller()
            ->GetPrimaryUserPrefService()
            ->GetInteger(chromeos::assistant::prefs::kAssistantConsentStatus));
  }
}

}  // namespace ash
