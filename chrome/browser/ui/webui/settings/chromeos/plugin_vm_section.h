// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_PLUGIN_VM_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_PLUGIN_VM_SECTION_H_

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_section.h"

class PrefService;

namespace content {
class WebUIDataSource;
}  // namespace content

namespace chromeos {
namespace settings {

// Provides UI strings for Plugin VM settings. This class class does not provide
// search tags, since this section will be removed eventually.
//
// TODO(https://crbug.com/1074101): Remove this class when the Plugin VM section
// of settings no longer exists.
class PluginVmSection : public OsSettingsSection {
 public:
  PluginVmSection(Profile* profile,
                  Delegate* per_page_delegate,
                  PrefService* pref_service);
  ~PluginVmSection() override;

 private:
  // OsSettingsSection:
  void AddLoadTimeData(content::WebUIDataSource* html_source) override;

  PrefService* pref_service_;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_PLUGIN_VM_SECTION_H_
