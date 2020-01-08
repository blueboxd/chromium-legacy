// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"

#include "base/logging.h"

namespace chromeos {

namespace app_time {

namespace {

std::string AppTypeToString(apps::mojom::AppType app_type) {
  switch (app_type) {
    case apps::mojom::AppType::kUnknown:
      return "Unknown";
    case apps::mojom::AppType::kArc:
      return "Arc";
    case apps::mojom::AppType::kWeb:
      return "Web";
    case apps::mojom::AppType::kExtension:
      return "Extension";
    case apps::mojom::AppType::kBuiltIn:
      return "Built in";
    case apps::mojom::AppType::kCrostini:
      return "Crostini";
    case apps::mojom::AppType::kMacNative:
      return "Mac native";
  }
  NOTREACHED();
}

}  // namespace

AppId::AppId(apps::mojom::AppType app_type, const std::string& app_id)
    : app_type_(app_type), app_id_(app_id) {
  DCHECK(!app_id.empty());
}

AppId::AppId(const AppId&) = default;

AppId& AppId::operator=(const AppId&) = default;

AppId::AppId(AppId&&) = default;

AppId& AppId::operator=(AppId&&) = default;

AppId::~AppId() = default;

bool AppId::operator==(const AppId& rhs) const {
  return app_type_ == rhs.app_type() && app_id_ == rhs.app_id();
}

bool AppId::operator!=(const AppId& rhs) const {
  return !(*this == rhs);
}

bool AppId::operator<(const AppId& rhs) const {
  return app_id_ < rhs.app_id();
}

std::ostream& operator<<(std::ostream& out, const AppId& id) {
  return out << " [" << AppTypeToString(id.app_type()) << " : " << id.app_id()
             << "]";
}

AppLimit::AppLimit(AppRestriction restriction,
                   base::Optional<base::TimeDelta> daily_limit,
                   base::Time last_updated)
    : restriction_(restriction),
      daily_limit_(daily_limit),
      last_updated_(last_updated) {
  DCHECK_EQ(restriction_ == AppRestriction::kBlocked,
            daily_limit_ == base::nullopt);
  DCHECK(daily_limit_ == base::nullopt ||
         daily_limit >= base::TimeDelta::FromHours(0));
  DCHECK(daily_limit_ == base::nullopt ||
         daily_limit <= base::TimeDelta::FromHours(24));
}

AppLimit::AppLimit(const AppLimit&) = default;

AppLimit& AppLimit::operator=(const AppLimit&) = default;

AppLimit::AppLimit(AppLimit&&) = default;

AppLimit& AppLimit::operator=(AppLimit&&) = default;

AppLimit::~AppLimit() = default;

AppActivity::ActiveTime::ActiveTime(base::Time start, base::Time end)
    : active_from_(start), active_to_(end) {
  DCHECK_GT(end, start);
}

AppActivity::ActiveTime::ActiveTime(const AppActivity::ActiveTime& rhs) =
    default;

AppActivity::ActiveTime& AppActivity::ActiveTime::operator=(
    const AppActivity::ActiveTime& rhs) = default;

AppActivity::AppActivity(AppState app_state)
    : app_state_(app_state),
      running_active_time_(base::TimeDelta::FromSeconds(0)),
      last_updated_time_ticks_(base::TimeTicks::Now()) {}
AppActivity::AppActivity(const AppActivity&) = default;
AppActivity& AppActivity::operator=(const AppActivity&) = default;
AppActivity::AppActivity(AppActivity&&) = default;
AppActivity& AppActivity::operator=(AppActivity&&) = default;
AppActivity::~AppActivity() = default;

void AppActivity::SetAppState(AppState app_state) {
  app_state_ = app_state;
  last_updated_time_ticks_ = base::TimeTicks::Now();
}

void AppActivity::SetAppActive(base::Time timestamp) {
  DCHECK(!is_active_);
  DCHECK(app_state_ == AppState::kAvailable ||
         app_state_ == AppState::kAlwaysAvailable);
  is_active_ = true;
  last_updated_time_ticks_ = base::TimeTicks::Now();
}

void AppActivity::SetAppInactive(base::Time timestamp) {
  if (!is_active_)
    return;

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta active_time = now - last_updated_time_ticks_;
  base::Time start_time = timestamp - active_time;

  is_active_ = false;
  active_times_.push_back(ActiveTime(start_time, timestamp));

  running_active_time_ += active_time;
  last_updated_time_ticks_ = now;
}

base::TimeDelta AppActivity::RunningActiveTime() const {
  if (!is_active_)
    return running_active_time_;

  return running_active_time_ +
         (base::TimeTicks::Now() - last_updated_time_ticks_);
}

}  // namespace app_time

}  // namespace chromeos
