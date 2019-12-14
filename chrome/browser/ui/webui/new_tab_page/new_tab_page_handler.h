// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/search/search_provider_observer.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/ntp_tile.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class GURL;
class PrefService;
class Profile;

class NewTabPageHandler : public content::WebContentsObserver,
                          public ntp_tiles::MostVisitedSites::Observer,
                          public new_tab_page::mojom::PageHandler {
 public:
  NewTabPageHandler(mojo::PendingReceiver<new_tab_page::mojom::PageHandler>
                        pending_page_handler,
                    mojo::PendingRemote<new_tab_page::mojom::Page> pending_page,
                    Profile* profile);
  ~NewTabPageHandler() override;

  // new_tab_page::mojom::PageHandler:
  void AddMostVisitedTile(const GURL& url,
                          const std::string& title,
                          AddMostVisitedTileCallback callback) override;
  void DeleteMostVisitedTile(const GURL& url,
                             DeleteMostVisitedTileCallback callback) override;
  void RestoreMostVisitedDefaults() override;
  void UpdateMostVisitedTile(const GURL& url,
                             const GURL& new_url,
                             const std::string& new_title,
                             UpdateMostVisitedTileCallback callback) override;
  void UndoMostVisitedTileAction() override;

 private:
  bool IsCustomLinksEnabled();
  void OnCustomLinksEnableChange();
  void OnNtpShortcutsVisibleChange();

  // ntp_tiles::MostVisitedSites::Observer implementation.
  void OnURLsAvailable(
      const std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector>&
          sections) override;
  void OnIconMadeAvailable(const GURL& site_url) override {}

  GURL last_blacklisted_;

  // Data source for NTP tiles (aka Most Visited tiles). May be null.
  std::unique_ptr<ntp_tiles::MostVisitedSites> most_visited_sites_;

  mojo::Remote<new_tab_page::mojom::Page> page_;

  PrefChangeRegistrar pref_change_registrar_;

  PrefService* pref_service_;

  mojo::Receiver<new_tab_page::mojom::PageHandler> receiver_;

  std::unique_ptr<SearchProviderObserver> search_provider_observer_;

  base::WeakPtrFactory<NewTabPageHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NewTabPageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_HANDLER_H_
