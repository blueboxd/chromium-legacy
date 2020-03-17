// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_PER_PAGE_STRINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_PER_PAGE_STRINGS_PROVIDER_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_concept.h"

class Profile;

namespace content {
class WebUIDataSource;
}  // namespace content

namespace chromeos {
namespace settings {

// Provides strings for an individual page in OS settings (i.e., each subpage is
// expected to have its own implementation. Responsible for two types of
// strings:
//
// (1) UI strings: Strings (e.g., headers, labels) displayed in the settings UI.
//     Added to a WebUIDataSource via the pure virtual AddUiStrings() function.
//
// (2) Search tags: Strings used as potential matches for user search queries
//     within settings. Added/removed via the {Add|Remove}SearchTagsGroup
//     delegate functions. Tags which are always searchable should be added in
//     the class' constructor; however, tags which apply to content which is
//     dynamically shown/hidden should be added when that content is visible and
//     removed when the content is no longer visible.
class OsSettingsPerPageStringsProvider {
 public:
  class Delegate {
   public:
    ~Delegate() = default;
    virtual void AddSearchTags(
        const std::vector<SearchConcept>& tags_group) = 0;
    virtual void RemoveSearchTags(
        const std::vector<SearchConcept>& tags_group) = 0;
  };

  virtual ~OsSettingsPerPageStringsProvider();

  OsSettingsPerPageStringsProvider(
      const OsSettingsPerPageStringsProvider& other) = delete;
  OsSettingsPerPageStringsProvider& operator=(
      const OsSettingsPerPageStringsProvider& other) = delete;

  // Adds strings to be displayed in the UI via loadTimeData.
  virtual void AddUiStrings(content::WebUIDataSource* html_source) const = 0;

 protected:
  static base::string16 GetHelpUrlWithBoard(const std::string& original_url);

  OsSettingsPerPageStringsProvider(Profile* profile, Delegate* delegate);

  Profile* profile() { return profile_; }
  Delegate* delegate() { return delegate_; }

 private:
  Profile* profile_;
  Delegate* delegate_;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_PER_PAGE_STRINGS_PROVIDER_H_
