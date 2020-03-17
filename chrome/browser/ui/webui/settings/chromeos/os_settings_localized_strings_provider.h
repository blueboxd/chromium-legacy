// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_LOCALIZED_STRINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_LOCALIZED_STRINGS_PROVIDER_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_per_page_strings_provider.h"
#include "chrome/services/local_search_service/public/mojom/local_search_service.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace content {
class WebUIDataSource;
}  // namespace content

namespace chromeos {
namespace settings {

struct SearchConcept;

// Provides two types of localized strings for OS settings:
//
// (1) UI strings: Strings displayed in the normal settings UI. This contains
//     strings such as headers, labels, instructional notes, etc. These strings
//     are added directly to the settings app's WebUIDataSource before the app
//     starts up via the static AddOsLocalizedStrings() function and are
//     accessible within settings via loadTimeData.
//
// (2) Search tags: Strings used as potential matches for user search queries
//     within settings. These strings don't appear in the normal UI; instead,
//     they specify actions which can be taken in settings. When a user types a
//     search query in settings, we compare the query against these strings to
//     look for potential matches. For each potential search result, there is a
//     "canonical" tag which represents a common phrase, and zero or more
//     alternate phrases (e.g., canonical: "Display settings", alternate:
//     "Monitor settings").
//
//     Since some of the settings sections may be unavailable (e.g., we don't
//     show Bluetooth settings unless the device has Bluetooth capabilities),
//     these strings are added/removed according to the Add/Remove*SearchTags()
//     instance functions.
class OsSettingsLocalizedStringsProvider
    : public KeyedService,
      public OsSettingsPerPageStringsProvider::Delegate {
 public:
  OsSettingsLocalizedStringsProvider(
      Profile* profile,
      local_search_service::mojom::LocalSearchService* local_search_service);
  OsSettingsLocalizedStringsProvider(
      const OsSettingsLocalizedStringsProvider& other) = delete;
  OsSettingsLocalizedStringsProvider& operator=(
      const OsSettingsLocalizedStringsProvider& other) = delete;
  ~OsSettingsLocalizedStringsProvider() override;

  // Adds the strings needed by the OS settings page to |html_source|
  // This function causes |html_source| to expose a strings.js file from its
  // source which contains a mapping from string's name to its translated value.
  void AddOsLocalizedStrings(content::WebUIDataSource* html_source,
                             Profile* profile);

  // Returns the tag metadata associated with |canonical_message_id|, which must
  // be one of the canonical IDS_SETTINGS_TAG_* identifiers used for a search
  // tag. If no metadata is available or if |canonical_message_id| instead
  // refers to an alternate tag's ID, null is returned.
  const SearchConcept* GetCanonicalTagMetadata(int canonical_message_id) const;

 private:
  // KeyedService:
  void Shutdown() override;

  // OsSettingsPerPageStringsProvider::Delegate:
  void AddSearchTags(const std::vector<SearchConcept>& tags_group) override;
  void RemoveSearchTags(const std::vector<SearchConcept>& tags_group) override;

  std::vector<std::unique_ptr<OsSettingsPerPageStringsProvider>>
      per_page_providers_;
  mojo::Remote<local_search_service::mojom::Index> index_remote_;
  std::unordered_map<int, const SearchConcept*> canonical_id_to_metadata_map_;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_LOCALIZED_STRINGS_PROVIDER_H_
