// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_AMBIENT_MODE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_AMBIENT_MODE_HANDLER_H_

#include <vector>

#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace ash {
struct AmbientSettings;
}  // namespace ash

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
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override;

 private:
  friend class AmbientModeHandlerTest;

  // WebUI call to request topic source related data.
  void HandleRequestTopicSource(const base::ListValue* args);

  // WebUI call to request albums related data.
  void HandleRequestAlbums(const base::ListValue* args);

  // WebUI call to sync topic source with server.
  void HandleSetSelectedTopicSource(const base::ListValue* args);

  // WebUI call to sync albums with server.
  void HandleSetSelectedAlbums(const base::ListValue* args);

  // Send the "topic-source-changed" WebUIListener event when the initial
  // settings is retrieved.
  void SendTopicSource();

  // Send the "albums-changed" WebUIListener event with albums info
  // in the |topic_source|.
  void SendAlbums(ash::AmbientModeTopicSource topic_source);

  // Update the local |settings_| to server.
  void UpdateSettings();

  // Called when the settings is updated.
  void OnUpdateSettings(bool success);

  void RequestSettingsAndAlbums(
      ash::AmbientBackendController::OnSettingsAndAlbumsFetchedCallback
          callback);

  // |topic_source| is what the |settings_| and |personal_albums_| were
  // requested for the ambientMode/photos subpage. It is base::nullopt if they
  // were requested by the ambientMode subpage.
  void OnSettingsAndAlbumsFetched(
      base::Optional<ash::AmbientModeTopicSource> topic_source,
      const base::Optional<ash::AmbientSettings>& settings,
      ash::PersonalAlbums personal_albums);

  base::Optional<ash::AmbientSettings> settings_;

  ash::PersonalAlbums personal_albums_;

  base::WeakPtrFactory<AmbientModeHandler> backend_weak_factory_{this};
  base::WeakPtrFactory<AmbientModeHandler> ui_update_weak_factory_{this};
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_AMBIENT_MODE_HANDLER_H_
