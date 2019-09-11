// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/voice_interaction/voice_interaction_controller_client.h"

#include <string>
#include <utility>

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "base/bind.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/assistant/assistant_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/arc/arc_util.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace arc {

namespace {

// Weak pointer.  This class is owned by ChromeBrowserMainPartsChromeos.
VoiceInteractionControllerClient*
    g_voice_interaction_controller_client_instance = nullptr;

}  // namespace

VoiceInteractionControllerClient::VoiceInteractionControllerClient() {
  DCHECK(!g_voice_interaction_controller_client_instance);
  g_voice_interaction_controller_client_instance = this;

  arc::ArcSessionManager::Get()->AddObserver(this);
  user_manager::UserManager::Get()->AddSessionStateObserver(this);

  assistant_state_ = ash::mojom::AssistantState::NOT_READY;
}

VoiceInteractionControllerClient::~VoiceInteractionControllerClient() {
  DCHECK_EQ(g_voice_interaction_controller_client_instance, this);
  g_voice_interaction_controller_client_instance = nullptr;

  user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
  arc::ArcSessionManager::Get()->RemoveObserver(this);
}

void VoiceInteractionControllerClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void VoiceInteractionControllerClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// static
VoiceInteractionControllerClient* VoiceInteractionControllerClient::Get() {
  return g_voice_interaction_controller_client_instance;
}

void VoiceInteractionControllerClient::NotifyStatusChanged(
    ash::mojom::AssistantState state) {
  assistant_state_ = state;
  ash::AssistantState::Get()->NotifyStatusChanged(state);
  for (auto& observer : observers_)
    observer.OnStateChanged(state);
}

void VoiceInteractionControllerClient::NotifyLockedFullScreenStateChanged(
    bool enabled) {
  ash::AssistantState::Get()->NotifyLockedFullScreenStateChanged(enabled);
}

void VoiceInteractionControllerClient::NotifyFeatureAllowed() {
  DCHECK(profile_);
  ash::mojom::AssistantAllowedState state =
      assistant::IsAssistantAllowedForProfile(profile_);
  ash::AssistantState::Get()->NotifyFeatureAllowed(state);
}

void VoiceInteractionControllerClient::NotifyLocaleChanged() {
  DCHECK(profile_);

  NotifyFeatureAllowed();

  std::string out_locale =
      profile_->GetPrefs()->GetString(language::prefs::kApplicationLocale);

  ash::AssistantState::Get()->NotifyLocaleChanged(out_locale);
}

void VoiceInteractionControllerClient::ActiveUserChanged(
    user_manager::User* active_user) {
  if (!active_user)
    return;

  active_user->AddProfileCreatedObserver(
      base::BindOnce(&VoiceInteractionControllerClient::SetProfileByUser,
                     weak_ptr_factory_.GetWeakPtr(), active_user));
}

void VoiceInteractionControllerClient::OnArcPlayStoreEnabledChanged(
    bool enabled) {
  ash::AssistantState::Get()->NotifyArcPlayStoreEnabledChanged(enabled);
}

void VoiceInteractionControllerClient::SetProfileByUser(
    const user_manager::User* user) {
  SetProfile(chromeos::ProfileHelper::Get()->GetProfileByUser(user));
}

void VoiceInteractionControllerClient::SetProfile(Profile* profile) {
  if (profile_ == profile)
    return;

  profile_ = profile;
  pref_change_registrar_.reset();

  if (!profile_)
    return;

  PrefService* prefs = profile->GetPrefs();
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);

  pref_change_registrar_->Add(
      language::prefs::kApplicationLocale,
      base::BindRepeating(
          &VoiceInteractionControllerClient::NotifyLocaleChanged,
          base::Unretained(this)));

  NotifyLocaleChanged();
  OnArcPlayStoreEnabledChanged(IsArcPlayStoreEnabledForProfile(profile_));
}

}  // namespace arc
