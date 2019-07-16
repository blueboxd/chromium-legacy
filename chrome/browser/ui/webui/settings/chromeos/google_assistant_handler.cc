// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/google_assistant_handler.h"

#include <utility>

#include "ash/public/cpp/assistant/assistant_settings.h"
#include "ash/public/cpp/assistant/assistant_setup.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/assistant_optin/assistant_optin_ui.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/services/assistant/public/mojom/constants.mojom.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "content/public/browser/browser_context.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/gfx/geometry/rect.h"

namespace chromeos {
namespace settings {

GoogleAssistantHandler::GoogleAssistantHandler(Profile* profile)
    : profile_(profile), weak_factory_(this) {
  chromeos::CrasAudioHandler::Get()->AddAudioObserver(this);
}

GoogleAssistantHandler::~GoogleAssistantHandler() {
  chromeos::CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

void GoogleAssistantHandler::OnJavascriptAllowed() {
  if (pending_hotword_update_) {
    OnAudioNodesChanged();
  }
}

void GoogleAssistantHandler::OnJavascriptDisallowed() {}

void GoogleAssistantHandler::OnAudioNodesChanged() {
  if (!IsJavascriptAllowed()) {
    pending_hotword_update_ = true;
    return;
  }

  pending_hotword_update_ = false;
  FireWebUIListener(
      "hotwordDeviceUpdated",
      base::Value(chromeos::CrasAudioHandler::Get()->HasHotwordDevice()));
}

void GoogleAssistantHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "showGoogleAssistantSettings",
      base::BindRepeating(
          &GoogleAssistantHandler::HandleShowGoogleAssistantSettings,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "retrainAssistantVoiceModel",
      base::BindRepeating(&GoogleAssistantHandler::HandleRetrainVoiceModel,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "syncVoiceModelStatus",
      base::BindRepeating(&GoogleAssistantHandler::HandleSyncVoiceModelStatus,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "initializeGoogleAssistantPage",
      base::BindRepeating(&GoogleAssistantHandler::HandleInitialized,
                          base::Unretained(this)));
}

void GoogleAssistantHandler::HandleShowGoogleAssistantSettings(
    const base::ListValue* args) {
  CHECK_EQ(0U, args->GetSize());
  if (chromeos::switches::IsAssistantEnabled())
    ash::OpenAssistantSettings();
}

void GoogleAssistantHandler::HandleRetrainVoiceModel(
    const base::ListValue* args) {
  CHECK_EQ(0U, args->GetSize());
  chromeos::AssistantOptInDialog::Show(ash::FlowType::kSpeakerIdRetrain,
                                       base::DoNothing());
}

void GoogleAssistantHandler::HandleSyncVoiceModelStatus(
    const base::ListValue* args) {
  CHECK_EQ(0U, args->GetSize());
  if (!settings_manager_.is_bound())
    BindAssistantSettingsManager();

  settings_manager_->SyncSpeakerIdEnrollmentStatus();
}

void GoogleAssistantHandler::HandleInitialized(const base::ListValue* args) {
  CHECK_EQ(0U, args->GetSize());
  AllowJavascript();
}

void GoogleAssistantHandler::BindAssistantSettingsManager() {
  DCHECK(!settings_manager_.is_bound());

  // Set up settings mojom.
  service_manager::Connector* connector =
      content::BrowserContext::GetConnectorFor(profile_);
  connector->BindInterface(assistant::mojom::kServiceName, &settings_manager_);
}

}  // namespace settings
}  // namespace chromeos
