// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_OS_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_OS_SETTINGS_PROVIDER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search.mojom.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

class Profile;

namespace apps {
class AppServiceProxy;
}  // namespace apps

namespace chromeos {
namespace settings {
class SearchHandler;
}
}  // namespace chromeos

namespace gfx {
class ImageSkia;
}

namespace app_list {

// Search results for OS settings.
class OsSettingsResult : public ChromeSearchResult {
 public:
  OsSettingsResult(Profile* profile,
                   const chromeos::settings::mojom::SearchResultPtr& result,
                   float relevance_score,
                   const gfx::ImageSkia& icon);
  ~OsSettingsResult() override;

  OsSettingsResult(const OsSettingsResult&) = delete;
  OsSettingsResult& operator=(const OsSettingsResult&) = delete;

  // ChromeSearchResult:
  void Open(int event_flags) override;
  ash::SearchResultType GetSearchResultType() const override;

 private:
  Profile* profile_;
  const std::string url_path_;
};

// Provider results for OS settings based on a search query. No results are
// provided for zero-state.
class OsSettingsProvider : public SearchProvider,
                           public apps::AppRegistryCache::Observer {
 public:
  explicit OsSettingsProvider(Profile* profile);
  ~OsSettingsProvider() override;

  OsSettingsProvider(const OsSettingsProvider&) = delete;
  OsSettingsProvider& operator=(const OsSettingsProvider&) = delete;

  // SearchProvider:
  void Start(const base::string16& query) override;
  ash::AppListSearchResultType ResultType() override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

 private:
  void OnSearchReturned(
      const base::string16& query,
      std::vector<chromeos::settings::mojom::SearchResultPtr> results);

  // Given a vector of results from the SearchHandler, filters them down to a
  // display-ready vector. It:
  // - returns at most |kMaxShownResults| results
  // - removes results with duplicate IDs
  // - removes results matching alternate text unless they meet extra
  // requirements
  //
  // The SearchHandler's vector is ranked high-to-low with this logic:
  // - compares SearchResultDefaultRank,
  // - if equal, compares relevance scores
  // - if equal, compares SearchResultType, favoring sections over subpages over
  //   settings
  // - if equal, picks arbitrarily
  //
  // So simply iterating down the vector while being careful about duplicates
  // and checking for alternate matches is enough.
  std::vector<chromeos::settings::mojom::SearchResultPtr> FilterResults(
      const base::string16& query,
      const std::vector<chromeos::settings::mojom::SearchResultPtr>& results);

  void OnLoadIcon(apps::mojom::IconValuePtr icon_value);

  // Scoring and filtering parameters controlled from Finch.
  size_t min_query_length_ = 1u;
  size_t min_query_length_for_alternates_ = 4u;
  float min_score_for_alternates_ = 0.35f;

  Profile* const profile_;
  chromeos::settings::SearchHandler* const search_handler_;
  apps::AppServiceProxy* app_service_proxy_;
  gfx::ImageSkia icon_;

  base::WeakPtrFactory<OsSettingsProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_OS_SETTINGS_PROVIDER_H_
