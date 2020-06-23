// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_TAB_SEARCH_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_TAB_SEARCH_PAGE_HANDLER_H_

#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/mojo_web_ui_controller.h"

class Browser;

class TabSearchPageHandler : public tab_search::mojom::PageHandler {
 public:
  TabSearchPageHandler(
      mojo::PendingReceiver<tab_search::mojom::PageHandler> receiver,
      mojo::PendingRemote<tab_search::mojom::Page> page,
      content::WebUI* web_ui);
  TabSearchPageHandler(const TabSearchPageHandler&) = delete;
  TabSearchPageHandler& operator=(const TabSearchPageHandler&) = delete;
  ~TabSearchPageHandler() override;

  // tab_search::mojom::PageHandler:
  void GetProfileTabs(GetProfileTabsCallback callback) override;
  void GetTabGroups(GetTabGroupsCallback callback) override;
  void SwitchToTab(
      tab_search::mojom::SwitchToTabInfoPtr switch_to_tab_info) override;

 private:
  tab_search::mojom::TabPtr GetTabData(Browser* browser,
                                       content::WebContents* contents,
                                       int index);

  mojo::Receiver<tab_search::mojom::PageHandler> receiver_;
  mojo::Remote<tab_search::mojom::Page> page_;
  Browser* const browser_;
  content::WebUI* const web_ui_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_TAB_SEARCH_PAGE_HANDLER_H_
