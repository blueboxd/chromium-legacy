// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"

#include <utility>

#include "base/callback.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_manager.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#endif

namespace web_app {

// static
std::unique_ptr<WebAppUiManager> WebAppUiManager::Create(Profile* profile) {
  return std::make_unique<WebAppUiManagerImpl>(profile);
}

WebAppUiManagerImpl::WebAppUiManagerImpl(Profile* profile) : profile_(profile) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    base::Optional<AppId> app_id = GetAppIdForBrowser(browser);
    if (!app_id.has_value())
      continue;

    ++num_windows_for_apps_map_[app_id.value()];
  }

  BrowserList::AddObserver(this);

  dialog_manager_ = std::make_unique<WebAppDialogManager>(profile);
}

WebAppUiManagerImpl::~WebAppUiManagerImpl() {
  BrowserList::RemoveObserver(this);
}

WebAppDialogManager& WebAppUiManagerImpl::dialog_manager() {
  return *dialog_manager_;
}

size_t WebAppUiManagerImpl::GetNumWindowsForApp(const AppId& app_id) {
  auto it = num_windows_for_apps_map_.find(app_id);
  if (it == num_windows_for_apps_map_.end())
    return 0;

  return it->second;
}

void WebAppUiManagerImpl::NotifyOnAllAppWindowsClosed(
    const AppId& app_id,
    base::OnceClosure callback) {
  const size_t num_windows_for_app = GetNumWindowsForApp(app_id);
  if (num_windows_for_app == 0) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
    return;
  }

  windows_closed_requests_map_[app_id].push_back(std::move(callback));
}

void WebAppUiManagerImpl::MigrateOSAttributes(const AppId& from,
                                              const AppId& to) {
#if defined(OS_CHROMEOS)
  app_list::AppListSyncableServiceFactory::GetForProfile(profile_)
      ->TransferItemAttributes(from, to);
#endif
}

void WebAppUiManagerImpl::OnBrowserAdded(Browser* browser) {
  base::Optional<AppId> app_id = GetAppIdForBrowser(browser);
  if (!app_id.has_value())
    return;

  ++num_windows_for_apps_map_[app_id.value()];
}

void WebAppUiManagerImpl::OnBrowserRemoved(Browser* browser) {
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

base::Optional<AppId> WebAppUiManagerImpl::GetAppIdForBrowser(
    Browser* browser) {
  if (browser->profile() != profile_)
    return base::nullopt;

  if (!browser->app_controller())
    return base::nullopt;

  return browser->app_controller()->GetAppId();
}

}  // namespace web_app
