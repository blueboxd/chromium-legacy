// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_MANAGER_IMPL_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_MANAGER_IMPL_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"

class Profile;
class Browser;

namespace web_app {

class WebAppProvider;
class WebAppDialogManager;

// This KeyedService is a UI counterpart for WebAppProvider.
class WebAppUiManagerImpl : public BrowserListObserver, public WebAppUiManager {
 public:
  static WebAppUiManagerImpl* Get(Profile* profile);

  explicit WebAppUiManagerImpl(Profile* profile);
  ~WebAppUiManagerImpl() override;

  // WebAppUiManager:
  WebAppDialogManager& dialog_manager() override;
  size_t GetNumWindowsForApp(const AppId& app_id) override;
  void NotifyOnAllAppWindowsClosed(const AppId& app_id,
                                   base::OnceClosure callback) override;
  void MigrateOSAttributes(const AppId& from, const AppId& to) override;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

 private:
  base::Optional<AppId> GetAppIdForBrowser(Browser* browser);

  std::unique_ptr<WebAppDialogManager> dialog_manager_;

  Profile* profile_;

  std::map<AppId, std::vector<base::OnceClosure>> windows_closed_requests_map_;
  std::map<AppId, size_t> num_windows_for_apps_map_;

  base::WeakPtrFactory<WebAppUiManagerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppUiManagerImpl);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_MANAGER_IMPL_H_
