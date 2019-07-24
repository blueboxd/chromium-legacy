// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/voice_interaction/voice_interaction_controller_client.h"

#include <string>
#include <utility>

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/voice_interaction_controller.h"
#include "base/bind.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/assistant/assistant_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_util.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
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

  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this, chrome::NOTIFICATION_SESSION_STARTED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                              content::NotificationService::AllSources());

  arc::ArcSessionManager::Get()->AddObserver(this);
  g_voice_interaction_controller_client_instance = this;

  if (chromeos::switches::IsAssistantEnabled()) {
    voice_interaction_state_ = ash::mojom::VoiceInteractionState::NOT_READY;
  }
}

VoiceInteractionControllerClient::~VoiceInteractionControllerClient() {
  DCHECK_EQ(g_voice_interaction_controller_client_instance, this);
  g_voice_interaction_controller_client_instance = nullptr;
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
    ash::mojom::VoiceInteractionState state) {
  voice_interaction_state_ = state;
  ash::VoiceInteractionController::Get()->NotifyStatusChanged(state);
  for (auto& observer : observers_)
    observer.OnStateChanged(state);
}

void VoiceInteractionControllerClient::NotifyLockedFullScreenStateChanged(
    bool enabled) {
  ash::VoiceInteractionController::Get()->NotifyLockedFullScreenStateChanged(
      enabled);
}

void VoiceInteractionControllerClient::NotifySettingsEnabled() {
  DCHECK(profile_);
  PrefService* prefs = profile_->GetPrefs();
  bool enabled = prefs->GetBoolean(prefs::kVoiceInteractionEnabled);
  ash::VoiceInteractionController::Get()->NotifySettingsEnabled(enabled);
}

void VoiceInteractionControllerClient::NotifyContextEnabled() {
  DCHECK(profile_);
  PrefService* prefs = profile_->GetPrefs();
  bool enabled = prefs->GetBoolean(prefs::kVoiceInteractionContextEnabled);
  ash::VoiceInteractionController::Get()->NotifyContextEnabled(enabled);
}

void VoiceInteractionControllerClient::NotifyHotwordEnabled() {
  DCHECK(profile_);
  PrefService* prefs = profile_->GetPrefs();
  bool enabled = prefs->GetBoolean(prefs::kVoiceInteractionHotwordEnabled);
  ash::VoiceInteractionController::Get()->NotifyHotwordEnabled(enabled);
}

void VoiceInteractionControllerClient::NotifyFeatureAllowed() {
  DCHECK(profile_);
  ash::mojom::AssistantAllowedState state =
      assistant::IsAssistantAllowedForProfile(profile_);
  ash::VoiceInteractionController::Get()->NotifyFeatureAllowed(state);
}

void VoiceInteractionControllerClient::NotifyLocaleChanged() {
  DCHECK(profile_);

  NotifyFeatureAllowed();

  std::string out_locale =
      profile_->GetPrefs()->GetString(language::prefs::kApplicationLocale);

  ash::VoiceInteractionController::Get()->NotifyLocaleChanged(out_locale);
}

void VoiceInteractionControllerClient::ActiveUserChanged(
    const user_manager::User* active_user) {
  if (active_user && active_user->is_profile_created())
    SetProfile(ProfileManager::GetActiveUserProfile());
}

void VoiceInteractionControllerClient::SetProfile(Profile* profile) {
  // Do nothing if this is called for the current profile. This can happen. For
  // example, ChromeSessionManager fires both
  // NOTIFICATION_LOGIN_USER_PROFILE_PREPARED and NOTIFICATION_SESSION_STARTED,
  // and we are observing both events.
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
  pref_change_registrar_->Add(
      prefs::kVoiceInteractionEnabled,
      base::BindRepeating(
          &VoiceInteractionControllerClient::NotifySettingsEnabled,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kVoiceInteractionContextEnabled,
      base::BindRepeating(
          &VoiceInteractionControllerClient::NotifyContextEnabled,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kVoiceInteractionHotwordEnabled,
      base::BindRepeating(
          &VoiceInteractionControllerClient::NotifyHotwordEnabled,
          base::Unretained(this)));

  NotifySettingsEnabled();
  NotifyContextEnabled();
  NotifyLocaleChanged();
  NotifyHotwordEnabled();
  OnArcPlayStoreEnabledChanged(IsArcPlayStoreEnabledForProfile(profile_));
}

void VoiceInteractionControllerClient::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED:
      // Update |profile_| when login user profile is prepared.
      // NOTIFICATION_SESSION_STARTED is not fired from UserSessionManager, but
      // profile may be changed by UserSessionManager in OOBE flow.
      SetProfile(ProfileManager::GetActiveUserProfile());
      break;
    case chrome::NOTIFICATION_SESSION_STARTED:
      // Update |profile_| when entering a session.
      SetProfile(ProfileManager::GetActiveUserProfile());

      // Add a session state observer to be able to monitor session changes.
      if (!session_state_observer_.get()) {
        session_state_observer_ =
            std::make_unique<user_manager::ScopedUserSessionStateObserver>(
                this);
      }
      break;
  }
}

void VoiceInteractionControllerClient::OnArcPlayStoreEnabledChanged(
    bool enabled) {
  ash::VoiceInteractionController::Get()->NotifyArcPlayStoreEnabledChanged(
      enabled);
}

}  // namespace arc
