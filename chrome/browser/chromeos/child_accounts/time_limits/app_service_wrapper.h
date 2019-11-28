// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_SERVICE_WRAPPER_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_SERVICE_WRAPPER_H_

#include <vector>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/services/app_service/public/cpp/app_registry_cache.h"

class Profile;

namespace apps {
class AppUpdate;
}  // namespace apps

namespace chromeos {
namespace app_time {

class AppId;

// Wrapper around AppService.
// Provides abstraction layer for Per-App Time Limits. Takes care of types
// conversions and data filetering, so those operations are not spread around
// the per-app time limits code.
class AppServiceWrapper : public apps::AppRegistryCache::Observer {
 public:
  // Notifies listeners about app state changes.
  class EventListener : public base::CheckedObserver {
   public:
    virtual void OnAppInstalled(const AppId& app_id) {}
    virtual void OnAppUninstalled(const AppId& app_id) {}
  };

  explicit AppServiceWrapper(Profile* profile);
  AppServiceWrapper(const AppServiceWrapper&) = delete;
  AppServiceWrapper& operator=(const AppServiceWrapper&) = delete;
  ~AppServiceWrapper() override;

  // Returns installed apps that are relevant for Per-App Time Limits feature.
  // Installed apps of unsupported types will not be included.
  std::vector<AppId> GetInstalledApps() const;

  void AddObserver(EventListener* observer);
  void RemoveObserver(EventListener* observer);

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

 private:
  apps::AppRegistryCache& GetAppCache() const;

  base::ObserverList<EventListener> listeners_;

  Profile* const profile_;
};

}  // namespace app_time
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_SERVICE_WRAPPER_H_
