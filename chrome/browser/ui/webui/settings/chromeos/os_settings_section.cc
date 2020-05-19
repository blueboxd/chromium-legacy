// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_section.h"

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_concept.h"

namespace chromeos {
namespace settings {

// static
base::string16 OsSettingsSection::GetHelpUrlWithBoard(
    const std::string& original_url) {
  return base::ASCIIToUTF16(original_url +
                            "&b=" + base::SysInfo::GetLsbReleaseBoard());
}

// static
void OsSettingsSection::RegisterNestedSettingBulk(
    mojom::Subpage subpage,
    const base::span<const mojom::Setting>& settings,
    HierarchyGenerator* generator) {
  for (const auto& setting : settings)
    generator->RegisterNestedSetting(setting, subpage);
}

OsSettingsSection::~OsSettingsSection() = default;

OsSettingsSection::OsSettingsSection(Profile* profile,
                                     SearchTagRegistry* search_tag_registry)
    : profile_(profile), search_tag_registry_(search_tag_registry) {
  DCHECK(profile);
  DCHECK(search_tag_registry);
}

OsSettingsSection::OsSettingsSection() = default;

std::string OsSettingsSection::ModifySearchResultUrl(
    const SearchConcept& concept) const {
  // Default case for static URLs which do not need to be modified.
  return concept.url_path_with_parameters;
}

}  // namespace settings
}  // namespace chromeos
