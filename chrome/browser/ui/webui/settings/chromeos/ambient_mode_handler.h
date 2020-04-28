// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_AMBIENT_MODE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_AMBIENT_MODE_HANDLER_H_

#include <vector>

#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace base {
class ListValue;
}  // namespace base

namespace chromeos {
namespace settings {

// Chrome OS ambient mode settings page UI handler, to allow users to customize
// photo frame and other related functionalities.
class AmbientModeHandler : public ::settings::SettingsPageUIHandler {
 public:
  AmbientModeHandler();
  AmbientModeHandler(const AmbientModeHandler&) = delete;
  AmbientModeHandler& operator=(const AmbientModeHandler&) = delete;
  ~AmbientModeHandler() override;

  // settings::SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override {}

 private:
  // WebUI call to signal js side is ready.
  void HandleInitialized(const base::ListValue* args);

  // WebUI call to sync topic source with server.
  void HandleTopicSourceSelectedChanged(const base::ListValue* args);

  // Retrieve the initial settings from server.
  void GetSettings();

  // Called when the initial settings is retrieved.
  void OnGetSettings(base::Optional<ash::AmbientModeTopicSource> topic_source);

  // Send the "topic-source-changed" WebUIListener event when the initial
  // settings is retrieved.
  void SendTopicSource();

  // Update the selected topic source to server.
  void UpdateSettings(ash::AmbientModeTopicSource topic_source);

  // Called when the settings is updated.
  // |topic_source| is the value to retry if the update was failed.
  void OnUpdateSettings(ash::AmbientModeTopicSource topic_source, bool success);

  // The topic source, i.e. from which category the photos will be displayed.
  base::Optional<ash::AmbientModeTopicSource> topic_source_;

  base::WeakPtrFactory<AmbientModeHandler> weak_factory_{this};
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_AMBIENT_MODE_HANDLER_H_
