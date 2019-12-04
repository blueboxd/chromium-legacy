// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_group_model.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_controller.h"
#include "chrome/browser/ui/tabs/tab_group_id.h"
#include "chrome/browser/ui/tabs/tab_group_visual_data.h"

TabGroupModel::TabGroupModel(TabGroupController* controller)
    : controller_(controller) {}
TabGroupModel::~TabGroupModel() {}

TabGroup* TabGroupModel::AddTabGroup(
    TabGroupId id,
    base::Optional<TabGroupVisualData> visual_data) {
  auto tab_group = std::make_unique<TabGroup>(
      controller_, id, visual_data.value_or(TabGroupVisualData()));
  groups_[id] = std::move(tab_group);

  return groups_[id].get();
}

bool TabGroupModel::ContainsTabGroup(TabGroupId id) const {
  return base::Contains(groups_, id);
}

TabGroup* TabGroupModel::GetTabGroup(TabGroupId id) const {
  DCHECK(ContainsTabGroup(id));
  return groups_.find(id)->second.get();
}

void TabGroupModel::RemoveTabGroup(TabGroupId id) {
  DCHECK(ContainsTabGroup(id));
  groups_.erase(id);
}

std::vector<TabGroupId> TabGroupModel::ListTabGroups() const {
  std::vector<TabGroupId> group_ids;
  group_ids.reserve(groups_.size());
  for (const auto& id_group_pair : groups_)
    group_ids.push_back(id_group_pair.first);
  return group_ids;
}
