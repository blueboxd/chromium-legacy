// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limits_whitelist_policy_wrapper.h"

namespace chromeos {
namespace app_time {

const char kUrlList[] = "url_list";
const char kAppList[] = "app_list";
const char kAppId[] = "app_id";
const char kAppType[] = "app_type";

std::string AppTypeToString(apps::mojom::AppType app_type) {
  switch (app_type) {
    case apps::mojom::AppType::kArc:
      return "ARC";
    case apps::mojom::AppType::kBuiltIn:
      return "BUILT-IN";
    case apps::mojom::AppType::kCrostini:
      return "CROSTINI";
    case apps::mojom::AppType::kExtension:
      return "EXTENSION";
    case apps::mojom::AppType::kWeb:
      return "WEB";
    default:
      NOTREACHED();
      return "";
  }
}

apps::mojom::AppType StringToAppType(const std::string& app_type) {
  if (app_type == "ARC")
    return apps::mojom::AppType::kArc;
  if (app_type == "BUILD-IN")
    return apps::mojom::AppType::kBuiltIn;
  if (app_type == "CROSTINI")
    return apps::mojom::AppType::kCrostini;
  if (app_type == "EXTENSION")
    return apps::mojom::AppType::kExtension;
  if (app_type == "WEB")
    return apps::mojom::AppType::kWeb;

  NOTREACHED();
  return apps::mojom::AppType::kUnknown;
}

AppTimeLimitsWhitelistPolicyWrapper::AppTimeLimitsWhitelistPolicyWrapper(
    const base::Value* value)
    : value_(value) {}

AppTimeLimitsWhitelistPolicyWrapper::~AppTimeLimitsWhitelistPolicyWrapper() =
    default;

std::vector<std::string>
AppTimeLimitsWhitelistPolicyWrapper::GetWhitelistURLList() const {
  const base::Value* list = value_->FindListKey(kUrlList);
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
  const base::Value* app_list = value_->FindListKey(kAppList);
  DCHECK(app_list);

  base::Value::ConstListView list_view = app_list->GetList();
  std::vector<AppId> return_value;
  for (const base::Value& value : list_view) {
    const std::string* app_id = value.FindStringKey(kAppId);
    DCHECK(app_id);

    const std::string* app_type = value.FindStringKey(kAppType);
    DCHECK(app_type);

    return_value.push_back(AppId(StringToAppType(*app_type), *app_id));
  }

  return return_value;
}

}  // namespace app_time
}  // namespace chromeos
