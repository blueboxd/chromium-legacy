// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_DLC_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_DLC_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"

namespace base {
class ListValue;
}  // namespace base

namespace chromeos {
namespace settings {

// Chrome OS Downloaded Content settings page UI handler.
class DlcHandler : public ::settings::SettingsPageUIHandler {
 public:
  DlcHandler();
  DlcHandler(const DlcHandler&) = delete;
  DlcHandler& operator=(const DlcHandler&) = delete;
  ~DlcHandler() override;

  // SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override;

 private:
  // Handler to get the latest list of DLCs.
  void HandleGetDlcList(const base::ListValue* args);

  // Handler to purge a DLC.
  void HandlePurgeDlc(const base::ListValue* args);

  void GetDlcListCallback(const base::Value& callback_id,
                          const std::string& err,
                          const dlcservice::DlcsWithContent& dlcs_with_content);

  void PurgeDlcCallback(const base::Value& callback_id, const std::string& err);

  base::WeakPtrFactory<DlcHandler> weak_ptr_factory_{this};
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_DLC_HANDLER_H_
