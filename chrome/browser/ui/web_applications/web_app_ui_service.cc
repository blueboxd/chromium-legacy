// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_ui_service.h"

#include <utility>

#include "base/callback.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_ui_service_factory.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#endif

namespace web_app {

// static
WebAppUiService* WebAppUiService::Get(Profile* profile) {
  return WebAppUiServiceFactory::GetForProfile(profile);
}

WebAppUiService::WebAppUiService(Profile* profile) : profile_(profile) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    base::Optional<AppId> app_id = GetAppIdForBrowser(browser);
    if (!app_id.has_value())
      continue;

    ++num_windows_for_apps_map_[app_id.value()];
  }

  BrowserList::AddObserver(this);
  auto* provider = WebAppProvider::Get(profile_);
  provider->set_ui_delegate(this);
}

WebAppUiService::~WebAppUiService() = default;

void WebAppUiService::Shutdown() {
  WebAppProvider::Get(profile_)->set_ui_delegate(nullptr);
  BrowserList::RemoveObserver(this);
}

size_t WebAppUiService::GetNumWindowsForApp(const AppId& app_id) {
  auto it = num_windows_for_apps_map_.find(app_id);
  if (it == num_windows_for_apps_map_.end())
    return 0;

  return it->second;
}

void WebAppUiService::NotifyOnAllAppWindowsClosed(const AppId& app_id,
                                                  base::OnceClosure callback) {
  const size_t num_windows_for_app = GetNumWindowsForApp(app_id);
  if (num_windows_for_app == 0) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
    return;
  }

  windows_closed_requests_map_[app_id].push_back(std::move(callback));
}

void WebAppUiService::MigrateOSAttributes(const AppId& from, const AppId& to) {
#if defined(OS_CHROMEOS)
  app_list::AppListSyncableServiceFactory::GetForProfile(profile_)
      ->TransferItemAttributes(from, to);
#endif
}

void WebAppUiService::OnBrowserAdded(Browser* browser) {
  base::Optional<AppId> app_id = GetAppIdForBrowser(browser);
  if (!app_id.has_value())
    return;

  ++num_windows_for_apps_map_[app_id.value()];
}

void WebAppUiService::OnBrowserRemoved(Browser* browser) {
  base::Optional<AppId> app_id_opt = GetAppIdForBrowser(browser);
  if (!app_id_opt.has_value())
    return;

  const auto& app_id = app_id_opt.value();

  size_t& num_windows_for_app = num_windows_for_apps_map_[app_id];
  DCHECK_GT(num_windows_for_app, 0u);
  --num_windows_for_app;

  if (num_windows_for_app > 0)
    return;

  auto it = windows_closed_requests_map_.find(app_id);
  if (it == windows_closed_requests_map_.end())
    return;

  for (auto& callback : it->second)
    std::move(callback).Run();

  windows_closed_requests_map_.erase(app_id);
}

base::Optional<AppId> WebAppUiService::GetAppIdForBrowser(Browser* browser) {
  if (browser->profile() != profile_)
    return base::nullopt;

  if (!browser->app_controller())
    return base::nullopt;

  return browser->app_controller()->GetAppId();
}

}  // namespace web_app
