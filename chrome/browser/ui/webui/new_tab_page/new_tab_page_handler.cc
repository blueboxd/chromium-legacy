// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ntp_tiles/chrome_most_visited_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

NewTabPageHandler::NewTabPageHandler(
    mojo::PendingReceiver<new_tab_page::mojom::PageHandler>
        pending_page_handler,
    mojo::PendingRemote<new_tab_page::mojom::Page> pending_page,
    Profile* profile)
    : page_{std::move(pending_page)},
      pref_service_(profile->GetPrefs()),
      receiver_{this, std::move(pending_page_handler)} {
  most_visited_sites_ = ChromeMostVisitedSitesFactory::NewForProfile(profile);
  // 9 tiles are required for the custom links feature in order to balance the
  // Most Visited rows (this is due to an additional "Add" button).
  most_visited_sites_->SetMostVisitedURLsObserver(this, 9);
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  if (template_url_service) {
    search_provider_observer_ = std::make_unique<SearchProviderObserver>(
        template_url_service,
        base::BindRepeating(&NewTabPageHandler::OnCustomLinksEnableChange,
                            weak_ptr_factory_.GetWeakPtr()));
  }
  most_visited_sites_->EnableCustomLinks(
      !pref_service_->GetBoolean(prefs::kNtpUseMostVisitedTiles));
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kNtpShortcutsVisible,
      base::BindRepeating(&NewTabPageHandler::OnNtpShortcutsVisibleChange,
                          weak_ptr_factory_.GetWeakPtr()));
  pref_change_registrar_.Add(
      prefs::kNtpUseMostVisitedTiles,
      base::BindRepeating(&NewTabPageHandler::OnCustomLinksEnableChange,
                          weak_ptr_factory_.GetWeakPtr()));
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
  bool success = true;
  if (IsCustomLinksEnabled()) {
    success = most_visited_sites_->DeleteCustomLink(url);
  } else {
    most_visited_sites_->AddOrRemoveBlacklistedUrl(url, true);
    last_blacklisted_ = url;
  }
  std::move(callback).Run(success);
}

bool NewTabPageHandler::IsCustomLinksEnabled() {
  return search_provider_observer_ && search_provider_observer_->is_google() &&
         !pref_service_->GetBoolean(prefs::kNtpUseMostVisitedTiles);
}

void NewTabPageHandler::RestoreMostVisitedDefaults() {
  if (IsCustomLinksEnabled()) {
    most_visited_sites_->UninitializeCustomLinks();
  } else {
    most_visited_sites_->ClearBlacklistedUrls();
  }
}

void NewTabPageHandler::ReorderMostVisitedTile(const GURL& url,
                                               uint8_t new_pos) {
  most_visited_sites_->ReorderCustomLink(url, new_pos);
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
  if (IsCustomLinksEnabled()) {
    most_visited_sites_->UndoCustomLinkAction();
  } else if (last_blacklisted_.is_valid()) {
    most_visited_sites_->AddOrRemoveBlacklistedUrl(last_blacklisted_, false);
    last_blacklisted_ = GURL();
  }
}

void NewTabPageHandler::OnCustomLinksEnableChange() {
  most_visited_sites_->EnableCustomLinks(IsCustomLinksEnabled());
  page_->SetCustomLinksEnabled(IsCustomLinksEnabled());
}

void NewTabPageHandler::OnNtpShortcutsVisibleChange() {
  page_->SetMostVisitedVisible(
      pref_service_->GetBoolean(prefs::kNtpShortcutsVisible));
}

void NewTabPageHandler::OnURLsAvailable(
    const std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector>&
        sections) {
  DCHECK(most_visited_sites_);
  std::vector<new_tab_page::mojom::MostVisitedTilePtr> list;
  auto info = new_tab_page::mojom::MostVisitedInfo::New();
  info->visible = pref_service_->GetBoolean(prefs::kNtpShortcutsVisible);
  // Use only personalized tiles for instant service.
  const ntp_tiles::NTPTilesVector& tiles =
      sections.at(ntp_tiles::SectionType::PERSONALIZED);
  for (const ntp_tiles::NTPTile& tile : tiles) {
    auto value = new_tab_page::mojom::MostVisitedTile::New();
    value->title = base::UTF16ToUTF8(tile.title);
    value->url = tile.url;
    list.push_back(std::move(value));
  }
  info->custom_links_enabled =
      !pref_service_->GetBoolean(prefs::kNtpUseMostVisitedTiles);
  info->tiles = std::move(list);
  page_->SetMostVisitedInfo(std::move(info));
}
