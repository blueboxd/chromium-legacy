// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_GOOGLE_ASSISTANT_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_GOOGLE_ASSISTANT_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/services/assistant/public/mojom/settings.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

class Profile;

namespace chromeos {
namespace settings {

class GoogleAssistantHandler : public ::settings::SettingsPageUIHandler {
 public:
  explicit GoogleAssistantHandler(Profile* profile);
  ~GoogleAssistantHandler() override;

  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  // WebUI call to launch into the Google Assistant app settings.
  void HandleShowGoogleAssistantSettings(const base::ListValue* args);
  // WebUI call to retrain Assistant voice model.
  void HandleRetrainVoiceModel(const base::ListValue* args);
  // WebUI call to sync Assistant voice model status.
  void HandleSyncVoiceModelStatus(const base::ListValue* args);

  // Bind to assistant settings manager.
  void BindAssistantSettingsManager();

  Profile* const profile_;

  assistant::mojom::AssistantSettingsManagerPtr settings_manager_;

  base::WeakPtrFactory<GoogleAssistantHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GoogleAssistantHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_GOOGLE_ASSISTANT_HANDLER_H_
