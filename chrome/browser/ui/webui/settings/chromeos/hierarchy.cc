// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/hierarchy.h"

#include "chrome/browser/ui/webui/settings/chromeos/constants/constants_util.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_section.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_sections.h"

namespace chromeos {
namespace settings {
namespace {

template <typename EnumType, typename MetadataType>
MetadataType& GetMetadata(EnumType enum_type,
                          mojom::Section section,
                          std::unordered_map<EnumType, MetadataType>* map) {
  auto it = map->find(enum_type);

  // Metadata already exists; return it.
  if (it != map->end())
    return it->second;

  // Metadata does not exist yet; insert then return it.
  auto pair = map->emplace(enum_type, section);
  CHECK(pair.second);
  return pair.first->second;
}

}  // namespace

class Hierarchy::PerSectionHierarchyGenerator
    : public OsSettingsSection::HierarchyGenerator {
 public:
  PerSectionHierarchyGenerator(mojom::Section section, Hierarchy* hierarchy)
      : section_(section), hierarchy_(hierarchy) {}

  void RegisterTopLevelSubpage(mojom::Subpage subpage) override {
    Hierarchy::SubpageMetadata& metadata = GetSubpageMetadata(subpage);
    CHECK_EQ(section_, metadata.section)
        << "Subpage registered in multiple sections: " << subpage;
  }

  void RegisterNestedSubpage(mojom::Subpage subpage,
                             mojom::Subpage parent_subpage) override {
    Hierarchy::SubpageMetadata& metadata = GetSubpageMetadata(subpage);
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
  Hierarchy::SubpageMetadata& GetSubpageMetadata(mojom::Subpage subpage) {
    return GetMetadata<mojom::Subpage, Hierarchy::SubpageMetadata>(
        subpage, section_, &hierarchy_->subpage_map_);
  }

  Hierarchy::SettingMetadata& GetSettingMetadata(mojom::Setting setting) {
    return GetMetadata<mojom::Setting, Hierarchy::SettingMetadata>(
        setting, section_, &hierarchy_->setting_map_);
  }

  mojom::Section section_;
  Hierarchy* hierarchy_;
};

Hierarchy::SubpageMetadata::SubpageMetadata(mojom::Section section)
    : section(section) {}

Hierarchy::SettingMetadata::SettingMetadata(mojom::Section primary_section)
    : primary(primary_section, /*subpage=*/base::nullopt) {}

Hierarchy::SettingMetadata::~SettingMetadata() = default;

Hierarchy::Hierarchy(const OsSettingsSections& sections) {
  for (const auto& section : constants::AllSections()) {
    PerSectionHierarchyGenerator generator(section, this);
    sections.GetSection(section)->RegisterHierarchy(&generator);
  }
}

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
