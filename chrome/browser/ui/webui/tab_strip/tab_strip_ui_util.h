// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_UTIL_H_

#include "base/optional.h"
#include "components/tab_groups/tab_group_id.h"

class Browser;
class Profile;
class TabGroupModel;

namespace tab_strip_ui {

base::Optional<tab_groups::TabGroupId> GetTabGroupIdFromString(
    TabGroupModel* tab_group_model,
    std::string group_id_string);

Browser* GetBrowserWithGroupId(Profile* profile, std::string group_id_string);

void MoveTabAcrossWindows(
    Browser* source_browser,
    int from_index,
    Browser* target_browser,
    int to_index,
    base::Optional<tab_groups::TabGroupId> to_group_id = base::nullopt);

}  // namespace tab_strip_ui

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_UTIL_H_
