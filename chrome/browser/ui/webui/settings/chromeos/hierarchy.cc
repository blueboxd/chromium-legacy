// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/hierarchy.h"

#include <utility>

#include "chrome/browser/ui/webui/settings/chromeos/constants/constants_util.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_section.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_sections.h"

namespace chromeos {
namespace settings {

class Hierarchy::PerSectionHierarchyGenerator
    : public OsSettingsSection::HierarchyGenerator {
 public:
  PerSectionHierarchyGenerator(mojom::Section section, Hierarchy* hierarchy)
      : section_(section), hierarchy_(hierarchy) {}

  void RegisterTopLevelSubpage(int name_message_id,
                               mojom::Subpage subpage) override {
    Hierarchy::SubpageMetadata& metadata =
        GetSubpageMetadata(name_message_id, subpage);
    CHECK_EQ(section_, metadata.section)
        << "Subpage registered in multiple sections: " << subpage;
  }

  void RegisterNestedSubpage(int name_message_id,
                             mojom::Subpage subpage,
                             mojom::Subpage parent_subpage) override {
    Hierarchy::SubpageMetadata& metadata =
        GetSubpageMetadata(name_message_id, subpage);
    CHECK_EQ(section_, metadata.section)
        << "Subpage registered in multiple sections: " << subpage;
    CHECK(!metadata.parent_subpage)
        << "Subpage has multiple registered parent subpages: " << subpage;
    metadata.parent_subpage = parent_subpage;
  }

  void RegisterTopLevelSetting(mojom::Setting setting) override {
    Hierarchy::SettingMetadata& metadata = GetSettingMetadata(setting);
    CHECK_EQ(section_, metadata.primary.first)
        << "Setting registered in multiple primary sections: " << setting;
    CHECK(!metadata.primary.second)
        << "Setting registered in multiple primary locations: " << setting;
  }

  void RegisterNestedSetting(mojom::Setting setting,
                             mojom::Subpage subpage) override {
    Hierarchy::SettingMetadata& metadata = GetSettingMetadata(setting);
    CHECK_EQ(section_, metadata.primary.first)
        << "Setting registered in multiple primary sections: " << setting;
    CHECK(!metadata.primary.second)
        << "Setting registered in multiple primary locations: " << setting;
    metadata.primary.second = subpage;
  }

  void RegisterTopLevelAltSetting(mojom::Setting setting) override {
    Hierarchy::SettingMetadata& metadata = GetSettingMetadata(setting);
    CHECK(metadata.primary.first != section_ || metadata.primary.second)
        << "Setting's primary and alternate locations are identical: "
        << setting;
    for (const auto& alternate : metadata.alternates) {
      CHECK(alternate.first != section_ || alternate.second)
          << "Setting has multiple identical alternate locations: " << setting;
    }
    metadata.alternates.emplace_back(section_, /*subpage=*/base::nullopt);
  }

  void RegisterNestedAltSetting(mojom::Setting setting,
                                mojom::Subpage subpage) override {
    Hierarchy::SettingMetadata& metadata = GetSettingMetadata(setting);
    CHECK(metadata.primary.first != section_ ||
          metadata.primary.second != subpage)
        << "Setting's primary and alternate locations are identical: "
        << setting;
    for (const auto& alternate : metadata.alternates) {
      CHECK(alternate.first != section_ || alternate.second != subpage)
          << "Setting has multiple identical alternate locations: " << setting;
    }
    metadata.alternates.emplace_back(section_, subpage);
  }

 private:
  Hierarchy::SubpageMetadata& GetSubpageMetadata(int name_message_id,
                                                 mojom::Subpage subpage) {
    auto& subpage_map = hierarchy_->subpage_map_;

    auto it = subpage_map.find(subpage);

    // Metadata already exists; return it.
    if (it != subpage_map.end())
      return it->second;

    // Metadata does not exist yet; insert then return it.
    auto pair = subpage_map.emplace(
        std::piecewise_construct, std::forward_as_tuple(subpage),
        std::forward_as_tuple(name_message_id, section_));
    CHECK(pair.second);
    return pair.first->second;
  }

  Hierarchy::SettingMetadata& GetSettingMetadata(mojom::Setting setting) {
    auto& settings_map = hierarchy_->setting_map_;

    auto it = settings_map.find(setting);

    // Metadata already exists; return it.
    if (it != settings_map.end())
      return it->second;

    // Metadata does not exist yet; insert then return it.
    auto pair = settings_map.emplace(setting, section_);
    CHECK(pair.second);
    return pair.first->second;
  }

  mojom::Section section_;
  Hierarchy* hierarchy_;
};

Hierarchy::SubpageMetadata::SubpageMetadata(int name_message_id,
                                            mojom::Section section)
    : name_message_id(name_message_id), section(section) {}

Hierarchy::SettingMetadata::SettingMetadata(mojom::Section primary_section)
    : primary(primary_section, /*subpage=*/base::nullopt) {}

Hierarchy::SettingMetadata::~SettingMetadata() = default;

Hierarchy::Hierarchy(const OsSettingsSections& sections) {
  for (const auto& section : constants::AllSections()) {
    PerSectionHierarchyGenerator generator(section, this);
    sections.GetSection(section)->RegisterHierarchy(&generator);
  }
}

Hierarchy::Hierarchy() = default;

Hierarchy::~Hierarchy() = default;

const Hierarchy::SubpageMetadata& Hierarchy::GetSubpageMetadata(
    mojom::Subpage subpage) const {
  const auto it = subpage_map_.find(subpage);
  CHECK(it != subpage_map_.end())
      << "Subpage missing from settings hierarchy: " << subpage;
  return it->second;
}

const Hierarchy::SettingMetadata& Hierarchy::GetSettingMetadata(
    mojom::Setting setting) const {
  const auto it = setting_map_.find(setting);
  CHECK(it != setting_map_.end())
      << "Setting missing from settings hierarchy: " << setting;
  return it->second;
}

}  // namespace settings
}  // namespace chromeos
