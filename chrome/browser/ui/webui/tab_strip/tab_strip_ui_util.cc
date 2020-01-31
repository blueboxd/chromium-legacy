// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "components/tab_groups/tab_group_id.h"

namespace tab_strip_ui {

base::Optional<tab_groups::TabGroupId> GetTabGroupIdFromString(
    TabGroupModel* tab_group_model,
    std::string group_id_string) {
  for (tab_groups::TabGroupId candidate : tab_group_model->ListTabGroups()) {
    if (candidate.ToString() == group_id_string) {
      return base::Optional<tab_groups::TabGroupId>{candidate};
    }
  }

  return base::nullopt;
}

Browser* GetBrowserWithGroupId(Profile* profile, std::string group_id_string) {
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile() != profile) {
      continue;
    }

    base::Optional<tab_groups::TabGroupId> group_id = GetTabGroupIdFromString(
        browser->tab_strip_model()->group_model(), group_id_string);
    if (group_id.has_value()) {
      return browser;
    }
  }

  return nullptr;
}

}  // namespace tab_strip_ui
