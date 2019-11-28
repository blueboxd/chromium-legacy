// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"

namespace chromeos {

namespace app_time {

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

AppActivity::AppActivity(AppState app_state)
    : app_state_(app_state), last_updated_(base::Time::Now()) {}
AppActivity::AppActivity(const AppActivity&) = default;
AppActivity& AppActivity::operator=(const AppActivity&) = default;
AppActivity::AppActivity(AppActivity&&) = default;
AppActivity& AppActivity::operator=(AppActivity&&) = default;
AppActivity::~AppActivity() = default;

void AppActivity::SetAppState(AppState app_state) {
  app_state_ = app_state;
  last_updated_ = base::Time::Now();
}

void AppActivity::SetActiveTime(base::TimeDelta active_time) {
  active_time_ = active_time;
  last_updated_ = base::Time::Now();
}

}  // namespace app_time

}  // namespace chromeos
