// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_SECTION_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_concept.h"

class Profile;

namespace content {
class WebUI;
class WebUIDataSource;
}  // namespace content

namespace chromeos {
namespace settings {

// Represents one top-level section of the settings app (i.e., one item on the
// settings UI navigation).
//
// When instantiated, an OsSettingsSection should track whether the section and
// any of its subpages or individual settings should be available to the user
// based on the current environment (user account details, device capabilities,
// flags, etc). If a section is available, search tags should be added for that
// section via AddSearchTags(), and if that section becomes unavailable, search
// tags should be removed via RemoveSearchTags().
//
// When the settings app is initialized, this class is used to add loadTimeData
// (e.g., localized strings and flag values) as well as SettingsPageUIHandlers
// (i.e., browser to JS IPC mechanisms) to the page.
class OsSettingsSection {
 public:
  class Delegate {
   public:
    ~Delegate() = default;
    virtual void AddSearchTags(
        const std::vector<SearchConcept>& tags_group) = 0;
    virtual void RemoveSearchTags(
        const std::vector<SearchConcept>& tags_group) = 0;
  };

  virtual ~OsSettingsSection();

  OsSettingsSection(const OsSettingsSection& other) = delete;
  OsSettingsSection& operator=(const OsSettingsSection& other) = delete;

  // Provides static data (i.e., localized strings and flag values) to an OS
  // settings instance.
  virtual void AddLoadTimeData(content::WebUIDataSource* html_source) = 0;

  // Adds SettingsPageUIHandlers to an OS settings instance. Override if the
  // derived type requires one or more handlers for this section.
  virtual void AddHandlers(content::WebUI* web_ui) {}

 protected:
  static base::string16 GetHelpUrlWithBoard(const std::string& original_url);

  OsSettingsSection(Profile* profile, Delegate* delegate);

  Profile* profile() { return profile_; }
  Delegate* delegate() { return delegate_; }

 private:
  Profile* profile_;
  Delegate* delegate_;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_SECTION_H_
