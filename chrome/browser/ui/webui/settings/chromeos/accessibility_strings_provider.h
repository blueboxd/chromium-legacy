// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_ACCESSIBILITY_STRINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_ACCESSIBILITY_STRINGS_PROVIDER_H_

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_per_page_strings_provider.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace content {
class WebUIDataSource;
}  // namespace content

namespace chromeos {
namespace settings {

// Provides UI strings and search tags for Accessibility settings.
class AccessibilityStringsProvider : public OsSettingsPerPageStringsProvider {
 public:
  AccessibilityStringsProvider(Profile* profile,
                               Delegate* per_page_delegate,
                               PrefService* pref_service);
  ~AccessibilityStringsProvider() override;

 private:
  // OsSettingsPerPageStringsProvider:
  void AddUiStrings(content::WebUIDataSource* html_source) override;

  void UpdateSearchTags();

  PrefService* pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_ACCESSIBILITY_STRINGS_PROVIDER_H_
