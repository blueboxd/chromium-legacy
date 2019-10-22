// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_manager.h>

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/chromeos/app_mode/kiosk_cryptohome_remover.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace chromeos {

namespace {

constexpr char kIconCacheDir[] = "web-kiosk/icon";

// This class is owned by ChromeBrowserMainPartsChromeos.
static WebKioskAppManager* g_web_kiosk_app_manager = nullptr;

}  // namespace

// static
const char WebKioskAppManager::kWebKioskDictionaryName[] = "web-kiosk";

// static
void WebKioskAppManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kWebKioskDictionaryName);
}

// static
WebKioskAppManager* WebKioskAppManager::Get() {
  CHECK(g_web_kiosk_app_manager);
  return g_web_kiosk_app_manager;
}

WebKioskAppManager::WebKioskAppManager()
    : auto_launch_account_id_(EmptyAccountId()), weak_ptr_factory_(this) {
  CHECK(!g_web_kiosk_app_manager);  // Only one instance is allowed.
  g_web_kiosk_app_manager = this;

  UpdateApps();
  local_accounts_subscription_ = CrosSettings::Get()->AddSettingsObserver(
      kAccountsPrefDeviceLocalAccounts,
      base::BindRepeating(&WebKioskAppManager::UpdateApps,
                          weak_ptr_factory_.GetWeakPtr()));
  local_account_auto_login_id_subscription_ =
      CrosSettings::Get()->AddSettingsObserver(
          kAccountsPrefDeviceLocalAccountAutoLoginId,
          base::BindRepeating(&WebKioskAppManager::UpdateApps,
                              weak_ptr_factory_.GetWeakPtr()));
}

WebKioskAppManager::~WebKioskAppManager() {
  g_web_kiosk_app_manager = nullptr;
}

WebKioskAppManager::SimpleWebAppData::SimpleWebAppData(
    const WebKioskAppData& data)
    : app_id(data.app_id()),
      account_id(data.account_id()),
      name(data.name()),
      icon(data.icon()),
      url(data.url()) {}

WebKioskAppManager::SimpleWebAppData::SimpleWebAppData()
    : account_id(EmptyAccountId()) {}

WebKioskAppManager::SimpleWebAppData::SimpleWebAppData(
    const SimpleWebAppData& other) = default;

WebKioskAppManager::SimpleWebAppData::~SimpleWebAppData() = default;

void WebKioskAppManager::GetKioskAppIconCacheDir(
    base::FilePath* cache_dir) const {
  base::FilePath user_data_dir;
  CHECK(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  *cache_dir = user_data_dir.AppendASCII(kIconCacheDir);
}

void WebKioskAppManager::OnKioskAppDataChanged(
    const std::string& app_id) const {
  NotifyWebKioskAppsChanged();
}

void WebKioskAppManager::OnKioskAppDataLoadFailure(
    const std::string& app_id) const {
  NotifyWebKioskAppsChanged();
}

void WebKioskAppManager::NotifyWebKioskAppsChanged() const {
  for (auto& observer : observers_)
    observer.OnWebKioskAppsChanged();
}

void WebKioskAppManager::GetApps(std::vector<SimpleWebAppData>* apps) const {
  apps->clear();
  apps->reserve(apps_.size());
  for (auto& app : apps_)
    apps->push_back(SimpleWebAppData(*app));
}

void WebKioskAppManager::AddObserver(WebKioskAppManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void WebKioskAppManager::RemoveObserver(WebKioskAppManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WebKioskAppManager::UpdateApps() {
  // Store current apps. We will compare old and new apps to determine which
  // apps are new, and which were deleted.
  std::map<std::string, std::unique_ptr<WebKioskAppData>> old_apps;
  for (auto& app : apps_)
    old_apps[app->app_id()] = std::move(app);
  apps_.clear();
  auto_launch_account_id_.clear();
  auto_launched_with_zero_delay_ = false;
  std::string auto_login_account_id_from_settings;
  CrosSettings::Get()->GetString(kAccountsPrefDeviceLocalAccountAutoLoginId,
                                 &auto_login_account_id_from_settings);

  // Re-populates |apps_| and reuses existing apps when possible.
  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(CrosSettings::Get());
  for (auto account : device_local_accounts) {
    if (account.type != policy::DeviceLocalAccount::TYPE_WEB_KIOSK_APP)
      continue;
    const AccountId account_id(AccountId::FromUserEmail(account.user_id));

    if (account.account_id == auto_login_account_id_from_settings) {
      auto_launch_account_id_ = account_id;
      int auto_launch_delay = 0;
      CrosSettings::Get()->GetInteger(
          kAccountsPrefDeviceLocalAccountAutoLoginDelay, &auto_launch_delay);
      auto_launched_with_zero_delay_ = auto_launch_delay == 0;
    }
    GURL url(account.web_kiosk_app_info.url());
    std::string app_id = web_app::GenerateAppIdFromURL(url);

    auto old_it = old_apps.find(app_id);
    if (old_it != old_apps.end()) {
      // TODO(apotapchuk): Data fetcher will be created, will use it to
      // update previously not loaded data.
      apps_.push_back(std::move(old_it->second));
      old_apps.erase(old_it);
    } else {
      apps_.push_back(std::make_unique<WebKioskAppData>(
          this, app_id, account_id, std::move(url)));
      apps_.back()->LoadFromCache();
    }

    KioskCryptohomeRemover::CancelDelayedCryptohomeRemoval(account_id);
  }

  ClearRemovedApps(std::move(old_apps));

  NotifyWebKioskAppsChanged();
}

void WebKioskAppManager::ClearRemovedApps(
    const std::map<std::string, std::unique_ptr<WebKioskAppData>>& old_apps) {
  std::vector<AccountId> account_ids_to_remove;
  account_ids_to_remove.reserve(old_apps.size());
  for (auto& entry : old_apps) {
    entry.second->ClearCache();
    account_ids_to_remove.push_back(entry.second->account_id());
  }
  KioskCryptohomeRemover::RemoveCryptohomesAndExitIfNeeded(
      account_ids_to_remove);
}

}  // namespace chromeos
