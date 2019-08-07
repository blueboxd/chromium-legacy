// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_prefs_controller.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/update/update_notification_controller.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash {

AssistantPrefsController::AssistantPrefsController()
    : session_observer_(this) {}

AssistantPrefsController::~AssistantPrefsController() = default;

void AssistantPrefsController::InitializeObserver(
    AssistantStateObserver* observer) {
  if (consent_status_.has_value())
    observer->OnAssistantConsentStatusChanged(consent_status_.value());
}

void AssistantPrefsController::UpdateState() {
  UpdateConsentStatus();
  UpdateHotwordAlwaysOn();
  UpdateLaunchWithMicOpen();
  UpdateNotificationEnabled();
}

void AssistantPrefsController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  pref_change_registrar_.reset();

  // Skip for non-primary user prefs.
  PrefService* primary_user_prefs =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  if (!primary_user_prefs || primary_user_prefs != pref_service)
    return;

  // Register preference changes.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      chromeos::assistant::prefs::kAssistantConsentStatus,
      base::BindRepeating(&AssistantPrefsController::UpdateConsentStatus,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      chromeos::assistant::prefs::kAssistantHotwordAlwaysOn,
      base::BindRepeating(&AssistantPrefsController::UpdateHotwordAlwaysOn,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      chromeos::assistant::prefs::kAssistantLaunchWithMicOpen,
      base::BindRepeating(&AssistantPrefsController::UpdateLaunchWithMicOpen,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      chromeos::assistant::prefs::kAssistantNotificationEnabled,
      base::BindRepeating(&AssistantPrefsController::UpdateNotificationEnabled,
                          base::Unretained(this)));

  UpdateState();
}

void AssistantPrefsController::UpdateConsentStatus() {
  auto consent_status = pref_change_registrar_->prefs()->GetInteger(
      chromeos::assistant::prefs::kAssistantConsentStatus);
  if (consent_status_.has_value() &&
      consent_status_.value() == consent_status) {
    return;
  }
  consent_status_ = consent_status;
  for (auto& observer : observers_)
    observer.OnAssistantConsentStatusChanged(consent_status_.value());
}

void AssistantPrefsController::UpdateHotwordAlwaysOn() {
  auto hotword_always_on = pref_change_registrar_->prefs()->GetBoolean(
      chromeos::assistant::prefs::kAssistantHotwordAlwaysOn);
  if (hotword_always_on_.has_value() &&
      hotword_always_on_.value() == hotword_always_on) {
    return;
  }
  hotword_always_on_ = hotword_always_on;
}

void AssistantPrefsController::UpdateLaunchWithMicOpen() {
  auto launch_with_mic_open = pref_change_registrar_->prefs()->GetBoolean(
      chromeos::assistant::prefs::kAssistantLaunchWithMicOpen);
  if (launch_with_mic_open_.has_value() &&
      launch_with_mic_open_.value() == launch_with_mic_open) {
    return;
  }
  launch_with_mic_open_ = launch_with_mic_open;
}

void AssistantPrefsController::UpdateNotificationEnabled() {
  auto notification_enabled = pref_change_registrar_->prefs()->GetBoolean(
      chromeos::assistant::prefs::kAssistantNotificationEnabled);
  if (notification_enabled_.has_value() &&
      notification_enabled_.value() == notification_enabled) {
    return;
  }
  notification_enabled_ = notification_enabled;
}

}  // namespace ash
