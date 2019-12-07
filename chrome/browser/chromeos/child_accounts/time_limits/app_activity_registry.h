// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_ACTIVITY_REGISTRY_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_ACTIVITY_REGISTRY_H_

#include <map>

#include "chrome/browser/chromeos/child_accounts/time_limits/app_service_wrapper.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"

namespace chromeos {
namespace app_time {

// Keeps track of app activity and time limits information.
// Stores app activity between user session. Information about uninstalled apps
// are removed from the registry after activity was uploaded to server or after
// 30 days if upload did not happen.
class AppActivityRegistry : public AppServiceWrapper::EventListener {
 public:
  explicit AppActivityRegistry(AppServiceWrapper* app_service_wrapper);
  AppActivityRegistry(const AppActivityRegistry&) = delete;
  AppActivityRegistry& operator=(const AppActivityRegistry&) = delete;
  ~AppActivityRegistry() override;

  // AppServiceWrapper::EventListener:
  void OnAppInstalled(const AppId& app_id) override;
  void OnAppUninstalled(const AppId& app_id) override;
  void OnAppAvailable(const AppId& app_id) override;
  void OnAppBlocked(const AppId& app_id) override;

 private:
  // Bundles detailed data stored for a specific app.
  struct AppDetails {
    AppDetails();
    explicit AppDetails(const AppActivity& activity);
    AppDetails(const AppDetails&);
    AppDetails& operator=(const AppDetails&);
    ~AppDetails();

    // Contains information about current app state and logged activity.
    AppActivity activity{AppState::kAvailable};

    // Contains information about restriction set for the app.
    base::Optional<AppLimit> limit;
  };

  // Adds an ap to the registry if it does not exist.
  void Add(const AppId& app_id);

  // Convenience methods to access state of the app identified by |app_id|.
  // Should only be called if app exists in the registry.
  AppState GetAppState(const AppId& app_id) const;
  void SetAppState(const AppId& app_id, AppState app_state);

  // Removes uninstalled apps from the registry. Should be called after the
  // recent data was successfully uploaded to server.
  void CleanRegistry();

  // Owned by AppTimeController.
  AppServiceWrapper* const app_service_wrapper_;

  std::map<AppId, AppDetails> activity_registry_;
};

}  // namespace app_time
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_ACTIVITY_REGISTRY_H_
