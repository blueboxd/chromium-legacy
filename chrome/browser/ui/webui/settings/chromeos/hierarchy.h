// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_HIERARCHY_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_HIERARCHY_H_

#include <unordered_map>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/setting.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_identifier.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search.mojom.h"

namespace chromeos {
namespace settings {

class OsSettingsSections;

// Tracks the OS settings page hierarchy. Settings is composed of a group of
// sections containing subpages and/or settings, and this class provides
// metadata for where these subpages/settings reside and what localized strings
// are used to describe them.
//
// A subpage can either be a direct child of a section or can be a nested
// subpage, meaning that its parent is another subpage.
//
// A setting can either be embedded as a direct child of a section or can be
// a child of a subpage. Additionally, some settings appear in multiple places.
// For example, the Wi-Fi on/off toggle appears in both the top-level Network
// section as well as the Wi-Fi subpage. In cases like this, we consider the
// "primary" location as the more-targeted one - in this example, the Wi-Fi
// subpage is the primary location of the toggle since it is more specific to
// Wi-Fi, and the alternate location is the one embedded in the Network section.
class Hierarchy {
 public:
  explicit Hierarchy(const OsSettingsSections* sections);
  Hierarchy(const Hierarchy& other) = delete;
  Hierarchy& operator=(const Hierarchy& other) = delete;
  virtual ~Hierarchy();

  class SubpageMetadata {
   public:
    SubpageMetadata(int name_message_id,
                    mojom::Section section,
                    mojom::Subpage subpage,
                    mojom::SearchResultIcon icon,
                    mojom::SearchResultDefaultRank default_rank,
                    const std::string& url_path_with_parameters,
                    const Hierarchy* hierarchy);
    ~SubpageMetadata();

    // The section in which the subpage appears.
    mojom::Section section;

    // The parent subpage, if applicable. Only applies to nested subpages.
    base::Optional<mojom::Subpage> parent_subpage;

    // Generates a search result for this subpage, using the canonical search
    // tag as the search result text. |relevance_score| must be passed by the
    // client, since this result is being created manually instead of via query
    // matching.
    mojom::SearchResultPtr ToSearchResult(double relevance_score) const;

   private:
    mojom::Subpage subpage_;

    // Message ID corresponding to the localized string used to describe this
    // subpage.
    int name_message_id_;

    // Icon used for this subpage.
    mojom::SearchResultIcon icon_;

    // Default rank; used to order returned results.
    mojom::SearchResultDefaultRank default_rank_;

    // Static URL path, which may need to be modified via
    // |modify_url_callback_|.
    std::string unmodified_url_path_with_parameters_;

    const Hierarchy* hierarchy_;
  };

  // The location of a setting, which includes its section and, if applicable,
  // its subpage. Some settings are embedded directly into the section and have
  // no associated subpage.
  using SettingLocation =
      std::pair<mojom::Section, base::Optional<mojom::Subpage>>;

  struct SettingMetadata {
    explicit SettingMetadata(mojom::Section primary_section);
    ~SettingMetadata();

    // The primary location, as described above.
    SettingLocation primary;

    // Alternate locations, as described above. Empty if the setting has no
    // alternate location.
    std::vector<SettingLocation> alternates;
  };

  // Generates a search result corresponding to |section|. |relevance_score|
  // must be passed by the client, since this result is being created manually
  // instead of via query matching.
  mojom::SearchResultPtr GenerateSectionSearchResult(
      mojom::Section section,
      double relevance_score) const;

  const SubpageMetadata& GetSubpageMetadata(mojom::Subpage subpage) const;
  const SettingMetadata& GetSettingMetadata(mojom::Setting setting) const;

 protected:
  // Used by tests.
  Hierarchy();

  std::unordered_map<mojom::Subpage, SubpageMetadata> subpage_map_;
  std::unordered_map<mojom::Setting, SettingMetadata> setting_map_;

 private:
  class PerSectionHierarchyGenerator;

  virtual std::string ModifySearchResultUrl(
      mojom::Section section,
      mojom::SearchResultType type,
      OsSettingsIdentifier id,
      const std::string& url_to_modify) const;

  const OsSettingsSections* sections_;
};

// TODO(https://crbug.com/1071700): Delete this function.
std::vector<base::string16> GenerateDummySettingsHierarchyStrings(
    const std::string& url_path_with_parameters);

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_HIERARCHY_H_
