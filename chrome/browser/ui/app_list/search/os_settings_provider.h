// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_OS_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_OS_SETTINGS_PROVIDER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"

class Profile;

namespace app_list {

// Search results for OS settings.
// TODO(crbug.com/1068851): This is still a WIP, and needs to integrate with the
// settings backend.
class OsSettingsResult : public ChromeSearchResult {
 public:
  OsSettingsResult(float relevance, Profile* profile);
  ~OsSettingsResult() override;

  OsSettingsResult(const OsSettingsResult&) = delete;
  OsSettingsResult& operator=(const OsSettingsResult&) = delete;

  // ChromeSearchResult overrides:
  void Open(int event_flags) override;
  ash::SearchResultType GetSearchResultType() const override;

 private:
  Profile* const profile_;
};

// Provider results for OS settings based on a search query. No results are
// provided for zero-state.
// TODO(crbug.com/1068851): This is still a WIP, and needs to integrate with the
// settings backend.
class OsSettingsProvider : public SearchProvider {
 public:
  explicit OsSettingsProvider(Profile* profile);
  ~OsSettingsProvider() override;

  OsSettingsProvider(const OsSettingsProvider&) = delete;
  OsSettingsProvider& operator=(const OsSettingsProvider&) = delete;

  void Start(const base::string16& query) override;

 private:
  Profile* const profile_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_OS_SETTINGS_PROVIDER_H_
