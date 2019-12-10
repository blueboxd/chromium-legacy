// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ntp_tiles/chrome_most_visited_sites_factory.h"
#include "chrome/browser/profiles/profile.h"

NewTabPageHandler::NewTabPageHandler(
    mojo::PendingReceiver<new_tab_page::mojom::PageHandler>
        pending_page_handler,
    mojo::PendingRemote<new_tab_page::mojom::Page> pending_page,
    Profile* profile)
    : page_{std::move(pending_page)},
      receiver_{this, std::move(pending_page_handler)} {
  most_visited_sites_ = ChromeMostVisitedSitesFactory::NewForProfile(profile);
  // 9 tiles are required for the custom links feature in order to balance the
  // Most Visited rows (this is due to an additional "Add" button).
  most_visited_sites_->SetMostVisitedURLsObserver(this, 9);
  most_visited_sites_->EnableCustomLinks(true);
}

NewTabPageHandler::~NewTabPageHandler() = default;

void NewTabPageHandler::AddMostVisitedTile(
    const GURL& url,
    const std::string& title,
    AddMostVisitedTileCallback callback) {
  bool success =
      most_visited_sites_->AddCustomLink(url, base::UTF8ToUTF16(title));
  std::move(callback).Run(success);
}

void NewTabPageHandler::DeleteMostVisitedTile(
    const GURL& url,
    DeleteMostVisitedTileCallback callback) {
  bool success = most_visited_sites_->DeleteCustomLink(url);
  std::move(callback).Run(success);
}

void NewTabPageHandler::RestoreMostVisitedDefaults() {
  most_visited_sites_->UninitializeCustomLinks();
}

void NewTabPageHandler::UpdateMostVisitedTile(
    const GURL& url,
    const GURL& new_url,
    const std::string& new_title,
    UpdateMostVisitedTileCallback callback) {
  bool success = most_visited_sites_->UpdateCustomLink(
      url, new_url != url ? new_url : GURL(), base::UTF8ToUTF16(new_title));
  std::move(callback).Run(success);
}

void NewTabPageHandler::UndoMostVisitedTileAction() {
  most_visited_sites_->UndoCustomLinkAction();
}

void NewTabPageHandler::OnURLsAvailable(
    const std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector>&
        sections) {
  DCHECK(most_visited_sites_);
  std::vector<new_tab_page::mojom::MostVisitedTilePtr> list;
  // Use only personalized tiles for instant service.
  const ntp_tiles::NTPTilesVector& tiles =
      sections.at(ntp_tiles::SectionType::PERSONALIZED);
  for (const ntp_tiles::NTPTile& tile : tiles) {
    auto value = new_tab_page::mojom::MostVisitedTile::New();
    value->title = base::UTF16ToUTF8(tile.title);
    value->url = tile.url;
    list.push_back(std::move(value));
  }
  page_->SetMostVisitedTiles(std::move(list));
}
