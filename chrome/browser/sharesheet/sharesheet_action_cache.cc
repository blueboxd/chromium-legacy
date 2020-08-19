// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_action_cache.h"

#include "chrome/browser/sharesheet/share_action.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"

namespace sharesheet {

SharesheetActionCache::SharesheetActionCache() = default;
// ShareActions will be initialised here by calling AddShareAction.

SharesheetActionCache::~SharesheetActionCache() = default;

const std::vector<std::unique_ptr<ShareAction>>&
SharesheetActionCache::GetShareActions() {
  return share_actions_;
}

ShareAction* SharesheetActionCache::GetActionFromName(
    const base::string16& action_name) {
  auto iter = share_actions_.begin();
  while (iter != share_actions_.end()) {
    if ((*iter)->GetActionName() == action_name) {
      return iter->get();
    } else {
      iter++;
    }
  }
  return nullptr;
}

void SharesheetActionCache::AddShareAction(
    std::unique_ptr<ShareAction> action) {
  DCHECK_EQ(action->GetActionIcon().size(), gfx::Size(kIconSize, kIconSize));
  share_actions_.push_back(std::move(action));
}

}  // namespace sharesheet
