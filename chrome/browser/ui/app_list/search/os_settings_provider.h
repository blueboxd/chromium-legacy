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

class Profile;

namespace chromeos {
namespace settings {
class SearchHandler;
}
}  // namespace chromeos

namespace app_list {

// Search results for OS settings.
class OsSettingsResult : public ChromeSearchResult {
 public:
  OsSettingsResult(Profile* profile,
                   const chromeos::settings::mojom::SearchResultPtr& result);
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
class OsSettingsProvider : public SearchProvider {
 public:
  explicit OsSettingsProvider(Profile* profile);
  ~OsSettingsProvider() override;

  OsSettingsProvider(const OsSettingsProvider&) = delete;
  OsSettingsProvider& operator=(const OsSettingsProvider&) = delete;

  // SearchProvider:
  void Start(const base::string16& query) override;

 private:
  void OnSearchReturned(
      std::vector<chromeos::settings::mojom::SearchResultPtr> results);

  Profile* const profile_;
  chromeos::settings::SearchHandler* const search_handler_;

  base::WeakPtrFactory<OsSettingsProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_OS_SETTINGS_PROVIDER_H_
