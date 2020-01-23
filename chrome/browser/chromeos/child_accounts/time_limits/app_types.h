// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TYPES_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TYPES_H_

#include <list>
#include <string>

#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"

namespace chromeos {
namespace app_time {

// Type of usage restriction that can be applied to the installed app.
enum class AppRestriction {
  kUnknown,
  // Installed app is not available for the user.
  kBlocked,
  // Daily time limit is enforced. Installed app will become unavailable for
  // the user after time limit is reached on a given day.
  kTimeLimit,
};

// State of the app. Used for activity recording and status reporting.
enum class AppState {
  // App is available for the user.
  kAvailable,
  // App cannot be restricted. Used for important system apps.
  kAlwaysAvailable,
  // App is not available for the user because of being blocked.
  kBlocked,
  // App is not available for the user because daily time limit was reached.
  kLimitReached,
  // App is uninstalled. Activity might still be preserved and reported for
  // recently uninstalled apps.
  kUninstalled,
};

// Identifies an app for app time limits.
// Different types of use different identifier format. ARC++ apps are identified
// by Android package name. Other types of apps use 32 character long Chrome
// specific app id.
class AppId {
 public:
  AppId(apps::mojom::AppType app_type, const std::string& app_id);
  AppId(const AppId&);
  AppId& operator=(const AppId&);
  AppId(AppId&&);
  AppId& operator=(AppId&&);
  ~AppId();

  apps::mojom::AppType app_type() const { return app_type_; }
  const std::string& app_id() const { return app_id_; }

  bool operator==(const AppId&) const;
  bool operator!=(const AppId&) const;
  bool operator<(const AppId&) const;
  friend std::ostream& operator<<(std::ostream&, const AppId&);

 private:
  apps::mojom::AppType app_type_ = apps::mojom::AppType::kUnknown;

  // Package name for |ARC| apps, 32 character long Chrome specific app id
  // otherwise.
  std::string app_id_;
};

// Represents restriction that can be applied to an installed app.
class AppLimit {
 public:
  // Creates AppLimit.
  // |daily_limit| can only be set when |restriction| is kTimeLimit.
  // |daily_limit| needs to be in range of [0, 24] hours.
  AppLimit(AppRestriction restriction,
           base::Optional<base::TimeDelta> daily_limit,
           base::Time last_updated);
  AppLimit(const AppLimit&);
  AppLimit& operator=(const AppLimit&);
  AppLimit(AppLimit&&);
  AppLimit& operator=(AppLimit&&);
  ~AppLimit();

  AppRestriction restriction() const { return restriction_; }
  base::Time last_updated() const { return last_updated_; }
  const base::Optional<base::TimeDelta>& daily_limit() const {
    return daily_limit_;
  }

 private:
  // Usage restriction applied to the app.
  AppRestriction restriction_ = AppRestriction::kUnknown;

  // Daily usage limit. Only set |restriction| is kTimeLimit.
  // Has to be between 0 and 24 hours.
  base::Optional<base::TimeDelta> daily_limit_;

  // UTC timestamp for the last time the limit was updated.
  base::Time last_updated_;
};

// Contains information about app usage.
class AppActivity {
 public:
  class ActiveTime {
   public:
    ActiveTime(base::Time start, base::Time end);
    ActiveTime(const ActiveTime& rhs);
    ActiveTime& operator=(const ActiveTime& rhs);

    bool operator==(const ActiveTime&) const;
    bool operator!=(const ActiveTime&) const;

    // Returns whether |timestamp| is included in this time period.
    bool Contains(base::Time timestamp) const;

    // Returns whether |timestamp| is earlier than this time period's start.
    bool IsEarlierThan(base::Time timestamp) const;

    // Returns whether |timestamp| is later than this time period's end.
    bool IsLaterThan(base::Time timestamp) const;

    base::Time active_from() const { return active_from_; }
    void set_active_from(base::Time active_from);
    base::Time active_to() const { return active_to_; }
    void set_active_to(base::Time active_to);

   private:
    base::Time active_from_;
    base::Time active_to_;
  };

  // Creates AppActivity and sets current |app_state_|.
  explicit AppActivity(AppState app_state);
  AppActivity(const AppActivity&);
  AppActivity& operator=(const AppActivity&);
  AppActivity(AppActivity&&);
  AppActivity& operator=(AppActivity&&);
  ~AppActivity();

  void SetAppState(AppState app_state);
  void SetAppActive(base::Time timestamp);
  void SetAppInactive(base::Time timestamp);

  void ResetRunningActiveTime() {
    running_active_time_ = base::TimeDelta::FromSeconds(0);
  }

  base::TimeDelta RunningActiveTime() const;

  // Removes active time data older than given |timestamp|.
  void RemoveActiveTimeEarlierThan(base::Time timestamp);

  bool is_active() const { return is_active_; }
  AppState app_state() const { return app_state_; }
  const std::list<ActiveTime>& active_times() const { return active_times_; }

 private:
  // boolean to specify if the application is active.
  bool is_active_ = false;

  // Current state of the app.
  // There might be relevant activity recoded for app that was uninstalled
  // recently.
  AppState app_state_ = AppState::kAvailable;

  // Keeps the sum of the active times since the last reset.
  base::TimeDelta running_active_time_;

  // The time app was active.
  std::list<ActiveTime> active_times_;

  // Time tick for the last time the activity was updated.
  base::TimeTicks last_updated_time_ticks_;
};

}  // namespace app_time
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TYPES_H_
