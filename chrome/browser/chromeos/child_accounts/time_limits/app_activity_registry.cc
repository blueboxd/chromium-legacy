// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/app_activity_registry.h"

#include "base/stl_util.h"
#include "base/time/default_tick_clock.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_notification_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "extensions/common/constants.h"

namespace chromeos {
namespace app_time {

namespace {

constexpr base::TimeDelta kFiveMinutes = base::TimeDelta::FromMinutes(5);
constexpr base::TimeDelta kOneMinute = base::TimeDelta::FromMinutes(1);
constexpr base::TimeDelta kZeroMinutes = base::TimeDelta::FromMinutes(0);

enterprise_management::App::AppType AppTypeForReporting(
    apps::mojom::AppType type) {
  switch (type) {
    case apps::mojom::AppType::kArc:
      return enterprise_management::App::ARC;
    case apps::mojom::AppType::kBuiltIn:
      return enterprise_management::App::BUILT_IN;
    case apps::mojom::AppType::kCrostini:
      return enterprise_management::App::CROSTINI;
    case apps::mojom::AppType::kExtension:
      return enterprise_management::App::EXTENSION;
    case apps::mojom::AppType::kWeb:
      return enterprise_management::App::WEB;
    default:
      return enterprise_management::App::UNKNOWN;
  }
}

enterprise_management::AppActivity::AppState AppStateForReporting(
    AppState state) {
  switch (state) {
    case AppState::kAvailable:
      return enterprise_management::AppActivity::DEFAULT;
    case AppState::kAlwaysAvailable:
      return enterprise_management::AppActivity::ALWAYS_AVAILABLE;
    case AppState::kBlocked:
      return enterprise_management::AppActivity::BLOCKED;
    case AppState::kLimitReached:
      return enterprise_management::AppActivity::LIMIT_REACHED;
    case AppState::kUninstalled:
      return enterprise_management::AppActivity::UNINSTALLED;
    default:
      return enterprise_management::AppActivity::UNKNOWN;
  }
}

}  // namespace

AppActivityRegistry::TestApi::TestApi(AppActivityRegistry* registry)
    : registry_(registry) {}

AppActivityRegistry::TestApi::~TestApi() = default;

const base::Optional<AppLimit>& AppActivityRegistry::TestApi::GetAppLimit(
    const AppId& app_id) const {
  DCHECK(base::Contains(registry_->activity_registry_, app_id));
  return registry_->activity_registry_.at(app_id).limit;
}

AppActivityRegistry::AppDetails::AppDetails() = default;

AppActivityRegistry::AppDetails::AppDetails(const AppActivity& activity)
    : activity(activity) {}

AppActivityRegistry::AppDetails::~AppDetails() = default;

AppActivityRegistry::AppActivityRegistry(
    AppServiceWrapper* app_service_wrapper,
    AppTimeNotificationDelegate* notification_delegate)
    : app_service_wrapper_(app_service_wrapper),
      notification_delegate_(notification_delegate) {
  DCHECK(app_service_wrapper_);
  DCHECK(notification_delegate_);
  app_service_wrapper_->AddObserver(this);
}

AppActivityRegistry::~AppActivityRegistry() {
  app_service_wrapper_->RemoveObserver(this);
}

void AppActivityRegistry::OnAppInstalled(const AppId& app_id) {
  // App might be already present in registry, because we preserve info between
  // sessions and app service does not. Make sure not to override cached state.
  if (!base::Contains(activity_registry_, app_id))
    Add(app_id);

  // TODO(agawronska): Update the limit from policy when new app is installed.
}

void AppActivityRegistry::OnAppUninstalled(const AppId& app_id) {
  // TODO(agawronska): Consider DCHECK instead of it. Not sure if there are
  // legit cases when we might go out of sync with AppService.
  if (base::Contains(activity_registry_, app_id))
    SetAppState(app_id, AppState::kUninstalled);
}

void AppActivityRegistry::OnAppAvailable(const AppId& app_id) {
  if (base::Contains(activity_registry_, app_id))
    SetAppState(app_id, AppState::kAvailable);
}

void AppActivityRegistry::OnAppBlocked(const AppId& app_id) {
  if (base::Contains(activity_registry_, app_id))
    SetAppState(app_id, AppState::kBlocked);
}

void AppActivityRegistry::OnAppActive(const AppId& app_id,
                                      aura::Window* window,
                                      base::Time timestamp) {
  if (!base::Contains(activity_registry_, app_id))
    return;

  AppDetails& app_details = activity_registry_[app_id];

  DCHECK(IsAppAvailable(app_id));

  std::set<aura::Window*>& active_windows = app_details.active_windows;

  active_windows.insert(window);

  // No need to set app as active if there were already active windows for the
  // app
  if (active_windows.size() > 1)
    return;

  SetAppActive(app_id, timestamp);
}

void AppActivityRegistry::OnAppInactive(const AppId& app_id,
                                        aura::Window* window,
                                        base::Time timestamp) {
  if (!base::Contains(activity_registry_, app_id))
    return;

  std::set<aura::Window*>& active_windows =
      activity_registry_[app_id].active_windows;

  if (!base::Contains(active_windows, window))
    return;

  active_windows.erase(window);
  if (active_windows.size() > 0)
    return;

  SetAppInactive(app_id, timestamp);
}

bool AppActivityRegistry::IsAppInstalled(const AppId& app_id) const {
  if (base::Contains(activity_registry_, app_id))
    return GetAppState(app_id) != AppState::kUninstalled;
  return false;
}

bool AppActivityRegistry::IsAppAvailable(const AppId& app_id) const {
  DCHECK(base::Contains(activity_registry_, app_id));
  auto state = GetAppState(app_id);
  return state == AppState::kAvailable || state == AppState::kAlwaysAvailable;
}

bool AppActivityRegistry::IsAppBlocked(const AppId& app_id) const {
  DCHECK(base::Contains(activity_registry_, app_id));
  return GetAppState(app_id) == AppState::kBlocked;
}

bool AppActivityRegistry::IsAppTimeLimitReached(const AppId& app_id) const {
  DCHECK(base::Contains(activity_registry_, app_id));
  return GetAppState(app_id) == AppState::kLimitReached;
}

bool AppActivityRegistry::IsAppActive(const AppId& app_id) const {
  DCHECK(base::Contains(activity_registry_, app_id));
  return activity_registry_.at(app_id).activity.is_active();
}

bool AppActivityRegistry::SetAppTimeLimitForTest(const AppId& app_id,
                                                 base::TimeDelta time_limit,
                                                 base::Time timestamp) {
  DCHECK(base::Contains(activity_registry_, app_id));
  AppDetails& details = activity_registry_.at(app_id);
  details.limit = AppLimit(AppRestriction::kTimeLimit, time_limit, timestamp);

  details.activity.set_last_notification(AppNotification::kUnknown);
  base::TimeDelta active_time = details.activity.RunningActiveTime();

  if (details.activity.is_active() && active_time < time_limit) {
    // The application still has some time before it reaches its time limit.
    if (details.app_limit_timer)
      details.app_limit_timer->AbandonAndStop();
    ScheduleTimeLimitCheckForApp(app_id);

    // No change in state.
    return false;
  }

  if (active_time < time_limit &&
      details.activity.app_state() == AppState::kLimitReached) {
    details.activity.SetAppState(AppState::kAvailable);
    return true;
  }

  if (active_time >= time_limit &&
      details.activity.app_state() == AppState::kAvailable) {
    details.activity.SetAppInactive(timestamp);
    details.activity.SetAppState(AppState::kLimitReached);
    return true;
  }
  return false;
}

base::TimeDelta AppActivityRegistry::GetActiveTime(const AppId& app_id) const {
  DCHECK(base::Contains(activity_registry_, app_id));
  return activity_registry_.at(app_id).activity.RunningActiveTime();
}

AppState AppActivityRegistry::GetAppState(const AppId& app_id) const {
  DCHECK(base::Contains(activity_registry_, app_id));
  return activity_registry_.at(app_id).activity.app_state();
}

AppActivityReportInterface::ReportParams
AppActivityRegistry::GenerateAppActivityReport(
    enterprise_management::ChildStatusReportRequest* report) const {
  // TODO(agawronska): We should also report the ongoing activity if it started
  // before the reporting, because it could have been going for a long time.
  const base::Time timestamp = base::Time::Now();
  bool anything_reported = false;

  for (const auto& entry : activity_registry_) {
    const AppId& app_id = entry.first;
    const AppActivity& registered_activity = entry.second.activity;

    // Do not report if there is no activity.
    if (registered_activity.active_times().empty())
      continue;

    enterprise_management::AppActivity* app_activity =
        report->add_app_activity();
    enterprise_management::App* app_info = app_activity->mutable_app_info();
    app_info->set_app_id(app_id.app_id());
    app_info->set_app_type(AppTypeForReporting(app_id.app_type()));
    // AppService is is only different for ARC++ apps.
    if (app_id.app_type() == apps::mojom::AppType::kArc) {
      app_info->add_additional_app_id(
          app_service_wrapper_->GetAppServiceId(app_id));
    }
    app_activity->set_app_state(
        AppStateForReporting(registered_activity.app_state()));
    app_activity->set_populated_at(timestamp.ToJavaTime());

    for (const auto& active_time : registered_activity.active_times()) {
      enterprise_management::TimePeriod* time_period =
          app_activity->add_active_time_periods();
      time_period->set_start_timestamp(active_time.active_from().ToJavaTime());
      time_period->set_end_timestamp(active_time.active_to().ToJavaTime());
    }
    anything_reported = true;
  }

  return AppActivityReportInterface::ReportParams{timestamp, anything_reported};
}

void AppActivityRegistry::CleanRegistry(base::Time timestamp) {
  for (auto it = activity_registry_.begin(); it != activity_registry_.end();) {
    const AppId& app_id = it->first;
    AppActivity& registered_activity = it->second.activity;
    // TODO(agawronska): Update data stored in user pref.
    registered_activity.RemoveActiveTimeEarlierThan(timestamp);
    // Remove app that was uninstalled and does not have any past activity
    // stored.
    if (GetAppState(app_id) == AppState::kUninstalled &&
        registered_activity.active_times().empty()) {
      it = activity_registry_.erase(it);
    } else {
      ++it;
    }
  }
}

void AppActivityRegistry::UpdateAppLimits(
    const std::map<AppId, AppLimit>& app_limits) {
  for (auto& entry : activity_registry_) {
    const AppId& app_id = entry.first;
    const base::Optional<AppLimit>& app_limit = entry.second.limit;
    if (!base::Contains(app_limits, app_id)) {
      if (app_limit)
        entry.second.limit = base::nullopt;
    } else {
      entry.second.limit = app_limits.at(app_id);
    }
  }
  // TODO(agawronska): Propagate information about the limit changes.
}

void AppActivityRegistry::OnResetTimeReached(base::Time timestamp) {
  for (std::pair<const AppId, AppDetails>& info : activity_registry_) {
    const AppId& app = info.first;
    AppDetails& details = info.second;

    // Reset running active time.
    details.activity.ResetRunningActiveTime(timestamp);

    // If timer is running, stop timer. Abandon all tasks set.
    if (details.app_limit_timer)
      details.app_limit_timer->AbandonAndStop();

    // If the time limit has been reached, mark the app as available.
    if (details.activity.app_state() == AppState::kLimitReached)
      details.activity.SetAppState(AppState::kAvailable);

    // If the application is currently active, schedule a time limit
    // check.
    if (details.activity.is_active())
      ScheduleTimeLimitCheckForApp(app);
  }
}

void AppActivityRegistry::Add(const AppId& app_id) {
  activity_registry_[app_id].activity = AppActivity(AppState::kAvailable);
}

void AppActivityRegistry::SetAppState(const AppId& app_id, AppState app_state) {
  DCHECK(base::Contains(activity_registry_, app_id));
  activity_registry_.at(app_id).activity.SetAppState(app_state);
}

void AppActivityRegistry::SetAppActive(const AppId& app_id,
                                       base::Time timestamp) {
  DCHECK(base::Contains(activity_registry_, app_id));
  AppDetails& app_details = activity_registry_[app_id];
  DCHECK(!app_details.activity.is_active());
  app_details.activity.SetAppActive(timestamp);

  // For web apps, chrome app will be set active, and it will have a timer
  // set.
  if (app_id.app_type() == apps::mojom::AppType::kWeb)
    return;

  ScheduleTimeLimitCheckForApp(app_id);
}

void AppActivityRegistry::SetAppInactive(const AppId& app_id,
                                         base::Time timestamp) {
  DCHECK(base::Contains(activity_registry_, app_id));
  auto& details = activity_registry_.at(app_id);

  details.activity.SetAppInactive(timestamp);
  if (details.app_limit_timer)
    details.app_limit_timer->AbandonAndStop();
}

void AppActivityRegistry::ScheduleTimeLimitCheckForApp(const AppId& app_id) {
  DCHECK(base::Contains(activity_registry_, app_id));
  AppDetails& app_details = activity_registry_[app_id];

  // If there is no time limit information, don't set the timer.
  if (!app_details.limit.has_value())
    return;

  const AppLimit& limit = app_details.limit.value();
  if (limit.restriction() != AppRestriction::kTimeLimit)
    return;

  if (!app_details.app_limit_timer) {
    app_details.app_limit_timer = std::make_unique<base::OneShotTimer>(
        base::DefaultTickClock::GetInstance());
  }

  DCHECK(!app_details.app_limit_timer->IsRunning());

  // Check that the timer instance has been created.
  base::Optional<base::TimeDelta> time_limit = GetTimeLeftForApp(app_id);
  DCHECK(time_limit.has_value());

  if (time_limit > kFiveMinutes) {
    time_limit = time_limit.value() - kFiveMinutes;
  } else if (time_limit > kOneMinute) {
    time_limit = time_limit.value() - kOneMinute;
  }

  VLOG(1) << "Schedule app time limit check for " << app_id << " for "
          << time_limit.value();

  app_details.app_limit_timer->Start(
      FROM_HERE, time_limit.value(),
      base::BindRepeating(&AppActivityRegistry::CheckTimeLimitForApp,
                          base::Unretained(this), app_id));
}

base::Optional<base::TimeDelta> AppActivityRegistry::GetTimeLeftForApp(
    const AppId& app_id) const {
  DCHECK(base::Contains(activity_registry_, app_id));
  const AppDetails& app_details = activity_registry_.at(app_id);

  // If |app_details.limit| doesn't have value, the app has no restriction.
  if (!app_details.limit.has_value())
    return base::nullopt;

  const AppLimit& limit = app_details.limit.value();

  if (limit.restriction() != AppRestriction::kTimeLimit)
    return base::nullopt;

  // If the app has kTimeLimit restriction, DCHECK that daily limit has value.
  DCHECK(limit.daily_limit().has_value());

  AppState state = app_details.activity.app_state();
  if (state == AppState::kAlwaysAvailable || state == AppState::kBlocked)
    return base::nullopt;

  if (state == AppState::kLimitReached)
    return kZeroMinutes;

  DCHECK(state == AppState::kAvailable);

  base::TimeDelta time_limit = limit.daily_limit().value();
  base::TimeDelta active_time = app_details.activity.RunningActiveTime();

  if (active_time >= time_limit)
    return kZeroMinutes;

  return time_limit - active_time;
}

void AppActivityRegistry::CheckTimeLimitForApp(const AppId& app_id) {
  AppDetails& details = activity_registry_[app_id];

  base::Optional<base::TimeDelta> time_left = GetTimeLeftForApp(app_id);
  AppNotification last_notification = details.activity.last_notification();

  if (!time_left.has_value())
    return;

  if (time_left <= kFiveMinutes && time_left > kOneMinute &&
      last_notification != AppNotification::kFiveMinutes) {
    notification_delegate_->ShowAppTimeLimitNotification(
        app_id, AppNotification::kFiveMinutes);
    details.activity.set_last_notification(AppNotification::kFiveMinutes);
    ScheduleTimeLimitCheckForApp(app_id);
    return;
  }

  if (time_left <= kOneMinute && time_left > kZeroMinutes &&
      last_notification != AppNotification::kOneMinute) {
    notification_delegate_->ShowAppTimeLimitNotification(
        app_id, AppNotification::kOneMinute);
    details.activity.set_last_notification(AppNotification::kOneMinute);
    ScheduleTimeLimitCheckForApp(app_id);
    return;
  }

  if (time_left == kZeroMinutes &&
      last_notification != AppNotification::kTimeLimitReached) {
    details.activity.set_last_notification(AppNotification::kTimeLimitReached);
    // Set app activity state as time limit reached.
    details.activity.SetAppInactive(base::Time::Now());
    details.activity.SetAppState(AppState::kLimitReached);
    notification_delegate_->ShowAppTimeLimitNotification(
        app_id, AppNotification::kTimeLimitReached);
  }
}

}  // namespace app_time
}  // namespace chromeos
