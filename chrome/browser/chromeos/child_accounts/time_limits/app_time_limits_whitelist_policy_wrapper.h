// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_LIMITS_WHITELIST_POLICY_WRAPPER_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_LIMITS_WHITELIST_POLICY_WRAPPER_H_

#include <string>
#include <vector>

#include "base/values.h"

namespace chromeos {
namespace app_time {

class AppId;

class AppTimeLimitsWhitelistPolicyWrapper {
 public:
  explicit AppTimeLimitsWhitelistPolicyWrapper(const base::Value* value);
  ~AppTimeLimitsWhitelistPolicyWrapper();

  // Delete copy constructor and copy assign operator.
  AppTimeLimitsWhitelistPolicyWrapper(
      const AppTimeLimitsWhitelistPolicyWrapper&) = delete;
  AppTimeLimitsWhitelistPolicyWrapper& operator=(
      const AppTimeLimitsWhitelistPolicyWrapper&) = delete;

  std::vector<std::string> GetWhitelistURLList() const;
  std::vector<AppId> GetWhitelistAppList() const;

 private:
  const base::Value* value_;
};

}  // namespace app_time
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_LIMITS_WHITELIST_POLICY_WRAPPER_H_
