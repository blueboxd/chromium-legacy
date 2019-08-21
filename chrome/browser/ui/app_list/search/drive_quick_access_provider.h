// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_DRIVE_QUICK_ACCESS_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_DRIVE_QUICK_ACCESS_PROVIDER_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"

class Profile;

namespace app_list {

// DriveQuickAccessProvider dispatches queries to extensions and fetches the
// results from them via chrome.launcherSearchProvider API.
class DriveQuickAccessProvider : public SearchProvider {
 public:
  explicit DriveQuickAccessProvider(Profile* profile);
  ~DriveQuickAccessProvider() override;

  // SearchProvider:
  void Start(const base::string16& query) override;

 private:
  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(DriveQuickAccessProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_DRIVE_QUICK_ACCESS_PROVIDER_H_
