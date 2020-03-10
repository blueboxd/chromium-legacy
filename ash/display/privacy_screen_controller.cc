// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/privacy_screen_controller.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/types/display_snapshot.h"

namespace ash {

PrivacyScreenController::PrivacyScreenController() {
  Shell::Get()->session_controller()->AddObserver(this);
  Shell::Get()->display_configurator()->AddObserver(this);
}

PrivacyScreenController::~PrivacyScreenController() {
  Shell::Get()->display_configurator()->RemoveObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
}

// static
void PrivacyScreenController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDisplayPrivacyScreenEnabled, false);
}

bool PrivacyScreenController::IsSupported() const {
  return Shell::Get()
      ->display_configurator()
      ->IsPrivacyScreenSupportedOnInternalDisplay();
}

bool PrivacyScreenController::IsManaged() const {
  return active_user_pref_service_->IsManagedPreference(
      prefs::kDisplayPrivacyScreenEnabled);
}

bool PrivacyScreenController::GetEnabled() const {
  return active_user_pref_service_ && active_user_pref_service_->GetBoolean(
                                          prefs::kDisplayPrivacyScreenEnabled);
}

void PrivacyScreenController::SetEnabled(bool enabled) {
  if (!IsSupported()) {
    LOG(ERROR) << "Attempted to set privacy-screen on an unsupported device.";
    return;
  }

  // Do not set the policy if it is managed by policy. However, we still want to
  // notify observers that a change was attempted in order to show a toast.
  if (IsManaged()) {
    for (Observer& observer : observers_)
      observer.OnPrivacyScreenSettingChanged(GetEnabled());
    return;
  }

  if (active_user_pref_service_) {
    active_user_pref_service_->SetBoolean(prefs::kDisplayPrivacyScreenEnabled,
                                          enabled);
  }
}

void PrivacyScreenController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PrivacyScreenController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PrivacyScreenController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  active_user_pref_service_ = pref_service;
  InitFromUserPrefs();
}

void PrivacyScreenController::OnSigninScreenPrefServiceInitialized(
    PrefService* prefs) {
  active_user_pref_service_ = prefs;

  // The privacy screen is toggled via commands to the GPU process, which is
  // initialized after the signin screen emits this event. Therefore we must
  // wait for OnDisplayModeChanged() to notify us when the display configuration
  // is ready, which implies that the GPU process and communication pipes are
  // ready.
  applying_login_screen_prefs_ = true;
}

void PrivacyScreenController::OnDisplayModeChanged(
    const std::vector<display::DisplaySnapshot*>& displays) {
  // OnDisplayModeChanged() may fire many times during Chrome's lifetime. We
  // limit it to when applying login screen prefs.
  if (applying_login_screen_prefs_) {
    InitFromUserPrefs();
    applying_login_screen_prefs_ = false;
  }
}

void PrivacyScreenController::OnEnabledPrefChanged(bool notify_observers) {
  if (IsSupported()) {
    const bool is_enabled = GetEnabled();
    Shell::Get()->display_configurator()->SetPrivacyScreenOnInternalDisplay(
        is_enabled);

    if (!notify_observers)
      return;

    for (Observer& observer : observers_)
      observer.OnPrivacyScreenSettingChanged(is_enabled);
  }
}

void PrivacyScreenController::InitFromUserPrefs() {
  DCHECK(active_user_pref_service_);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(active_user_pref_service_);
  pref_change_registrar_->Add(
      prefs::kDisplayPrivacyScreenEnabled,
      base::BindRepeating(&PrivacyScreenController::OnEnabledPrefChanged,
                          base::Unretained(this),
                          /*notify_observers=*/true));

  // We don't want to notify observers upon initialization or on account change
  // because changes will trigger a toast to show up.
  OnEnabledPrefChanged(/*notify_observers=*/false);
}

}  // namespace ash
