// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_SERVICE_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_SERVICE_H_

#include <map>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/web_applications/components/web_app_ui_delegate.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;
class Browser;

namespace web_app {

// This KeyedService is a UI counterpart for WebAppProvider.
class WebAppUiService : public KeyedService,
                        public BrowserListObserver,
                        public WebAppUiDelegate {
 public:
  static WebAppUiService* Get(Profile* profile);

  explicit WebAppUiService(Profile* profile);
  ~WebAppUiService() override;

  // KeyedService
  void Shutdown() override;

  // WebAppUiDelegate
  size_t GetNumWindowsForApp(const AppId& app_id) override;
  void NotifyOnAllAppWindowsClosed(const AppId& app_id,
                                   base::OnceClosure callback) override;
  void MigrateOSAttributes(const AppId& from, const AppId& to) override;

  // BrowserListObserver
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

 private:
  base::Optional<AppId> GetAppIdForBrowser(Browser* browser);

  Profile* profile_;

  std::map<AppId, std::vector<base::OnceClosure>> windows_closed_requests_map_;
  std::map<AppId, size_t> num_windows_for_apps_map_;

  base::WeakPtrFactory<WebAppUiService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppUiService);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_SERVICE_H_
