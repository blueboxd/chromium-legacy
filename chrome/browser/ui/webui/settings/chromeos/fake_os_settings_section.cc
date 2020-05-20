// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/fake_os_settings_section.h"

#include <sstream>

#include "chrome/grit/generated_resources.h"

namespace chromeos {
namespace settings {

FakeOsSettingsSection::FakeOsSettingsSection(mojom::Section section)
    : section_(section) {}

FakeOsSettingsSection::~FakeOsSettingsSection() = default;

int FakeOsSettingsSection::GetSectionNameMessageId() const {
  return IDS_INTERNAL_APP_SETTINGS;
}

std::string FakeOsSettingsSection::ModifySearchResultUrl(
    const SearchConcept& concept) const {
  std::stringstream ss;
  ss << section_ << "::" << concept.url_path_with_parameters;
  return ss.str();
}

}  // namespace settings
}  // namespace chromeos
