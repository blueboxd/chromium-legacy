// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SERVICE_APP_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SERVICE_APP_RESULT_H_

#include <memory>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/app_list/search/app_result.h"
#include "chrome/services/app_service/public/cpp/icon_cache.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"

class AppListControllerDelegate;
class Profile;

namespace app_list {

class AppServiceAppResult : public AppResult {
 public:
  AppServiceAppResult(Profile* profile,
                      const std::string& app_id,
                      AppListControllerDelegate* controller,
                      bool is_recommendation,
                      apps::IconLoader* icon_loader);
  ~AppServiceAppResult() override;

 private:
  // ChromeSearchResult overrides:
  void Open(int event_flags) override;
  void GetContextMenuModel(GetMenuModelCallback callback) override;
  ash::SearchResultType GetSearchResultType() const override;
  AppContextMenu* GetAppContextMenu() override;

  // AppContextMenuDelegate overrides:
  void ExecuteLaunchCommand(int event_flags) override;

  void Launch(int event_flags, apps::mojom::LaunchSource launch_source);

  void CallLoadIcon(bool chip, bool allow_placeholder_icon);
  void OnLoadIcon(bool chip, apps::mojom::IconValuePtr icon_value);

  apps::IconLoader* const icon_loader_;

  // When non-nullptr, signifies that this object is using the most recent icon
  // fetched from |icon_loader_|. When destroyed, informs |icon_loader_| that
  // the last icon is no longer used.
  std::unique_ptr<apps::IconLoader::Releaser> icon_loader_releaser_;

  apps::mojom::AppType app_type_;
  bool is_platform_app_;
  bool show_in_launcher_;

  std::unique_ptr<AppContextMenu> context_menu_;

  base::WeakPtrFactory<AppServiceAppResult> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppServiceAppResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SERVICE_APP_RESULT_H_
