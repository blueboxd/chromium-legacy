// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limits_whitelist_policy_wrapper.h"

#include "base/optional.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_policy_helpers.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"

namespace chromeos {
namespace app_time {

AppTimeLimitsWhitelistPolicyWrapper::AppTimeLimitsWhitelistPolicyWrapper(
    const base::Value* value)
    : value_(value) {}

AppTimeLimitsWhitelistPolicyWrapper::~AppTimeLimitsWhitelistPolicyWrapper() =
    default;

std::vector<std::string>
AppTimeLimitsWhitelistPolicyWrapper::GetWhitelistURLList() const {
  const base::Value* list = value_->FindListKey(policy::kUrlList);
  DCHECK(list);

  base::Value::ConstListView list_view = list->GetList();
  std::vector<std::string> return_value;
  for (const base::Value& value : list_view) {
    return_value.push_back(value.GetString());
  }
  return return_value;
}

std::vector<AppId> AppTimeLimitsWhitelistPolicyWrapper::GetWhitelistAppList()
    const {
  const base::Value* app_list = value_->FindListKey(policy::kAppList);
  DCHECK(app_list);

  base::Value::ConstListView list_view = app_list->GetList();
  std::vector<AppId> return_value;
  for (const base::Value& value : list_view) {
    base::Optional<AppId> app_id = policy::AppIdFromDict(value);
    if (app_id)
      return_value.push_back(*app_id);
  }

  return return_value;
}

}  // namespace app_time
}  // namespace chromeos
