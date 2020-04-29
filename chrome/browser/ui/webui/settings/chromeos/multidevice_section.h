// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_MULTIDEVICE_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_MULTIDEVICE_SECTION_H_

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_section.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace chromeos {
namespace settings {

// Provides UI strings and search tags for MultiDevice settings. Different
// search tags are registered depending on whether MultiDevice features are
// allowed and whether the user has opted into the suite of features.
class MultiDeviceSection
    : public OsSettingsSection,
      public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  MultiDeviceSection(
      Profile* profile,
      Delegate* per_page_delegate,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client);
  ~MultiDeviceSection() override;

 private:
  // OsSettingsSection:
  void AddLoadTimeData(content::WebUIDataSource* html_source) override;

  // multidevice_setup::MultiDeviceSetupClient::Observer:
  void OnHostStatusChanged(
      const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
          host_status_with_device) override;

  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_MULTIDEVICE_SECTION_H_
