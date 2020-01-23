// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_CONTROLLER_H_

#include <memory>

#include "base/time/time.h"

class Profile;
class PrefRegistrySimple;
class PrefChangeRegistrar;
class PrefService;
class Profile;

namespace chromeos {
namespace app_time {

class AppActivityRegistry;
class AppServiceWrapper;
class WebTimeLimitEnforcer;

// Coordinates per-app time limit for child user.
class AppTimeController {
 public:
  // Used for tests to get internal implementation details.
  class TestApi {
   public:
    explicit TestApi(AppTimeController* controller);
    ~TestApi();

    AppActivityRegistry* app_registry();

   private:
    AppTimeController* const controller_;
  };

  static bool ArePerAppTimeLimitsEnabled();
  static bool IsAppActivityReportingEnabled();

  // Registers preferences
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit AppTimeController(Profile* profile);
  AppTimeController(const AppTimeController&) = delete;
  AppTimeController& operator=(const AppTimeController&) = delete;
  ~AppTimeController();

  bool IsExtensionWhitelisted(const std::string& extension_id) const;

  const WebTimeLimitEnforcer* web_time_enforcer() const {
    return web_time_enforcer_.get();
  }

  WebTimeLimitEnforcer* web_time_enforcer() { return web_time_enforcer_.get(); }

  const AppActivityRegistry* app_registry() const {
    return app_registry_.get();
  }

  AppActivityRegistry* app_registry() { return app_registry_.get(); }

 private:
  void RegisterProfilePrefObservers(PrefService* pref_service);
  void TimeLimitsPolicyUpdated(const std::string& pref_name);
  void TimeLimitsWhitelistPolicyUpdated(const std::string& pref_name);

  // The time of the day when app time limits should be reset.
  // Defaults to 6am.
  base::TimeDelta limits_reset_time_ = base::TimeDelta::FromHours(6);

  std::unique_ptr<AppServiceWrapper> app_service_wrapper_;
  std::unique_ptr<AppActivityRegistry> app_registry_;
  std::unique_ptr<WebTimeLimitEnforcer> web_time_enforcer_;

  // Used to observe when policy preferences change.
  std::unique_ptr<PrefChangeRegistrar> pref_registrar_;
};

}  // namespace app_time
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_CONTROLLER_H_
