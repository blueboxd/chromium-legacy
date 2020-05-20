// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_HIERARCHY_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_HIERARCHY_H_

#include <unordered_map>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/setting.mojom.h"

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
  explicit Hierarchy(const OsSettingsSections& sections);
  Hierarchy(const Hierarchy& other) = delete;
  Hierarchy& operator=(const Hierarchy& other) = delete;
  virtual ~Hierarchy();

  struct SubpageMetadata {
    SubpageMetadata(int name_message_id, mojom::Section section);

    // Message ID corresponding to the localized string used to describe this
    // subpage.
    int name_message_id;

    // The section in which the subpage appears.
    mojom::Section section;

    // The parent subpage, if applicable. Only applies to nested subpages.
    base::Optional<mojom::Subpage> parent_subpage;
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

  const SubpageMetadata& GetSubpageMetadata(mojom::Subpage subpage) const;
  const SettingMetadata& GetSettingMetadata(mojom::Setting setting) const;

 protected:
  // Used by tests.
  Hierarchy();

  std::unordered_map<mojom::Subpage, SubpageMetadata> subpage_map_;
  std::unordered_map<mojom::Setting, SettingMetadata> setting_map_;

 private:
  class PerSectionHierarchyGenerator;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_HIERARCHY_H_
