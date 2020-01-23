// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_ACTIVITY_REGISTRY_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_ACTIVITY_REGISTRY_H_

#include <map>
#include <set>

#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_activity_report_interface.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_service_wrapper.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"

namespace aura {
class Window;
}  // namespace aura

namespace enterprise_management {
class ChildStatusReportRequest;
}  // namespace enterprise_management

namespace chromeos {
namespace app_time {

// Keeps track of app activity and time limits information.
// Stores app activity between user session. Information about uninstalled apps
// are removed from the registry after activity was uploaded to server or after
// 30 days if upload did not happen.
class AppActivityRegistry : public AppServiceWrapper::EventListener {
 public:
  // Used for tests to get internal implementation details.
  class TestApi {
   public:
    explicit TestApi(AppActivityRegistry* registry);
    ~TestApi();

    const base::Optional<AppLimit>& GetAppLimit(const AppId& app_id) const;

   private:
    AppActivityRegistry* const registry_;
  };

  explicit AppActivityRegistry(AppServiceWrapper* app_service_wrapper);
  AppActivityRegistry(const AppActivityRegistry&) = delete;
  AppActivityRegistry& operator=(const AppActivityRegistry&) = delete;
  ~AppActivityRegistry() override;

  // AppServiceWrapper::EventListener:
  void OnAppInstalled(const AppId& app_id) override;
  void OnAppUninstalled(const AppId& app_id) override;
  void OnAppAvailable(const AppId& app_id) override;
  void OnAppBlocked(const AppId& app_id) override;
  void OnAppActive(const AppId& app_id,
                   aura::Window* window,
                   base::Time timestamp) override;
  void OnAppInactive(const AppId& app_id,
                     aura::Window* window,
                     base::Time timestamp) override;

  bool IsAppInstalled(const AppId& app_id) const;
  bool IsAppAvailable(const AppId& app_id) const;
  bool IsAppBlocked(const AppId& app_id) const;
  bool IsAppTimeLimitReached(const AppId& app_id) const;
  bool IsAppActive(const AppId& app_id) const;

  // Returns the total active time for the application since the last time limit
  // reset.
  base::TimeDelta GetActiveTime(const AppId& app_id) const;

  // Populates |report| with collected app activity. Returns whether any data
  // were reported.
  AppActivityReportInterface::ReportParams GenerateAppActivityReport(
      enterprise_management::ChildStatusReportRequest* report) const;

  // Removes data older than |timestamp| from the registry.
  // Removes entries for uninstalled apps if there is no more relevant activity
  // data left.
  void CleanRegistry(base::Time timestamp);

  // Updates time limits for the installed apps.
  void UpdateAppLimits(const std::map<AppId, AppLimit>& app_limits);

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

    // Contains the set of active windows for the application.
    std::set<aura::Window*> active_windows;

    // Contains information about restriction set for the app.
    base::Optional<AppLimit> limit;
  };

  // Adds an ap to the registry if it does not exist.
  void Add(const AppId& app_id);

  // Convenience methods to access state of the app identified by |app_id|.
  // Should only be called if app exists in the registry.
  AppState GetAppState(const AppId& app_id) const;
  void SetAppState(const AppId& app_id, AppState app_state);

  // Methods to set the application as active and inactive respectively.
  void SetAppActive(const AppId& app_id, base::Time timestamp);
  void SetAppInactive(const AppId& app_id, base::Time timestamp);

  // Owned by AppTimeController.
  AppServiceWrapper* const app_service_wrapper_;

  std::map<AppId, AppDetails> activity_registry_;
};

}  // namespace app_time
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_ACTIVITY_REGISTRY_H_
