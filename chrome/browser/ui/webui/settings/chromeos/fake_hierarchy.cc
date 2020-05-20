// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/fake_hierarchy.h"

#include <utility>

namespace chromeos {
namespace settings {

FakeHierarchy::FakeHierarchy() = default;

FakeHierarchy::~FakeHierarchy() = default;

void FakeHierarchy::AddSubpageMetadata(
    int name_message_id,
    mojom::Section section,
    mojom::Subpage subpage,
    base::Optional<mojom::Subpage> parent_subpage) {
  auto pair = subpage_map_.emplace(
      std::piecewise_construct, std::forward_as_tuple(subpage),
      std::forward_as_tuple(name_message_id, section));
  DCHECK(pair.second);
  pair.first->second.parent_subpage = parent_subpage;
}

void FakeHierarchy::AddSettingMetadata(
    mojom::Section section,
    mojom::Setting setting,
    base::Optional<mojom::Subpage> parent_subpage) {
  auto pair = setting_map_.emplace(setting, section);
  DCHECK(pair.second);
  pair.first->second.primary.second = parent_subpage;
}

}  // namespace settings
}  // namespace chromeos
