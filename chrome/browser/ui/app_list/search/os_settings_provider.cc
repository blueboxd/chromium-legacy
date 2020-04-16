// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/os_settings_provider.h"

#include <memory>
#include <string>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_handler_factory.h"
#include "url/gurl.h"

namespace app_list {
namespace {

constexpr char kOsSettingsResultPrefix[] = "os-settings://";

}  // namespace

OsSettingsResult::OsSettingsResult(
    Profile* profile,
    const chromeos::settings::mojom::SearchResultPtr& result)
    : profile_(profile), url_path_(result->url_path_with_parameters) {
  // TODO(crbug.com/1068851): Results need a useful relevance score and details
  // text. Once this is available in the SearchResultPtr, set the metadata here.
  set_id(kOsSettingsResultPrefix + url_path_);
  set_relevance(8.0f);
  SetTitle(result->result_text);
  SetResultType(ResultType::kOsSettings);
  SetDisplayType(DisplayType::kList);
  // TODO(crbug.com/1068851): Set the icon for the result.
}

OsSettingsResult::~OsSettingsResult() = default;

void OsSettingsResult::Open(int event_flags) {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(profile_,
                                                               url_path_);
}

ash::SearchResultType OsSettingsResult::GetSearchResultType() const {
  return ash::OS_SETTINGS;
}

OsSettingsProvider::OsSettingsProvider(Profile* profile)
    : profile_(profile),
      search_handler_(
          chromeos::settings::SearchHandlerFactory::GetForProfile(profile)) {
  DCHECK(profile_);
  DCHECK(search_handler_);
}

OsSettingsProvider::~OsSettingsProvider() = default;

void OsSettingsProvider::Start(const base::string16& query) {
  // This provider does not handle zero-state.
  if (query.empty())
    return;

  // Invalidate weak pointers to cancel existing searches.
  weak_factory_.InvalidateWeakPtrs();
  // TODO(crbug.com/1068851): There are currently only a handful of settings
  // returned from the backend. Once the search service has finished integration
  // into settings, verify we see all results here, and that opening works
  // correctly for the new URLs.
  search_handler_->Search(query,
                          base::BindOnce(&OsSettingsProvider::OnSearchReturned,
                                         weak_factory_.GetWeakPtr()));
}

void OsSettingsProvider::OnSearchReturned(
    std::vector<chromeos::settings::mojom::SearchResultPtr> results) {
  SearchProvider::Results search_results;
  for (const auto& result : results) {
    search_results.emplace_back(
        std::make_unique<OsSettingsResult>(profile_, result));
  }
  SwapResults(&search_results);
}

}  // namespace app_list
