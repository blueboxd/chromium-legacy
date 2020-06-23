// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_search/tab_search_page_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/webui/util/image_util.h"

TabSearchPageHandler::TabSearchPageHandler(
    mojo::PendingReceiver<tab_search::mojom::PageHandler> receiver,
    mojo::PendingRemote<tab_search::mojom::Page> page,
    content::WebUI* web_ui)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      browser_(chrome::FindLastActive()),
      web_ui_(web_ui) {
  DCHECK(browser_);
}

TabSearchPageHandler::~TabSearchPageHandler() = default;

void TabSearchPageHandler::GetProfileTabs(GetProfileTabsCallback callback) {
  auto profile_tabs = tab_search::mojom::ProfileTabs::New();
  Profile* profile = browser_->profile();
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile() != profile) {
      continue;
    }
    TabStripModel* tab_strip_model = browser->tab_strip_model();
    auto window_tabs = tab_search::mojom::WindowTabs::New();
    window_tabs->active = (browser == browser_);
    for (int i = 0; i < tab_strip_model->count(); ++i) {
      window_tabs->tabs.push_back(
          GetTabData(browser, tab_strip_model->GetWebContentsAt(i), i));
    }
    profile_tabs->windows.push_back(std::move(window_tabs));
  }

  std::move(callback).Run(std::move(profile_tabs));
}

void TabSearchPageHandler::GetTabGroups(GetTabGroupsCallback callback) {
  // TODO(crbug.com/1096120): Implement this when we can get theme color from
  // browser
  NOTIMPLEMENTED();
}

void TabSearchPageHandler::SwitchToTab(
    tab_search::mojom::SwitchToTabInfoPtr switch_to_tab_info) {
  Profile* profile = browser_->profile();
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile() != profile) {
      continue;
    }
    TabStripModel* tab_strip_model = browser->tab_strip_model();
    for (int index = 0; index < tab_strip_model->count(); ++index) {
      content::WebContents* contents = tab_strip_model->GetWebContentsAt(index);
      if (extensions::ExtensionTabUtil::GetTabId(contents) ==
          switch_to_tab_info->tab_id) {
        tab_strip_model->ActivateTabAt(index);
        browser->window()->Activate();
        return;
      }
    }
  }
}

tab_search::mojom::TabPtr TabSearchPageHandler::GetTabData(
    Browser* browser,
    content::WebContents* contents,
    int index) {
  auto tab_data = tab_search::mojom::Tab::New();

  tab_data->active = browser->tab_strip_model()->active_index() == index;
  tab_data->tab_id = extensions::ExtensionTabUtil::GetTabId(contents);
  tab_data->index = index;

  const base::Optional<tab_groups::TabGroupId> group_id =
      browser->tab_strip_model()->GetTabGroupForTab(index);
  if (group_id.has_value()) {
    tab_data->group_id = group_id.value().ToString();
  }

  TabRendererData tab_renderer_data =
      TabRendererData::FromTabInModel(browser->tab_strip_model(), index);
  tab_data->pinned = tab_renderer_data.pinned;
  tab_data->title = base::UTF16ToUTF8(tab_renderer_data.title);
  tab_data->url = tab_renderer_data.visible_url.GetContent();

  if (tab_renderer_data.favicon.isNull()) {
    tab_data->is_default_favicon = true;
  } else {
    tab_data->fav_icon_url = webui::EncodePNGAndMakeDataURI(
        tab_renderer_data.favicon, web_ui_->GetDeviceScaleFactor());
    tab_data->is_default_favicon =
        tab_renderer_data.favicon.BackedBySameObjectAs(
            favicon::GetDefaultFavicon().AsImageSkia());
  }
  tab_data->show_icon = tab_renderer_data.show_icon;

  return tab_data;
}
