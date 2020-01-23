// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_LIMITS_WHITELIST_POLICY_TEST_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_LIMITS_WHITELIST_POLICY_TEST_UTILS_H_

#include <string>

#include "base/values.h"

namespace chromeos {
namespace app_time {

class AppId;

class AppTimeLimitsWhitelistPolicyBuilder {
 public:
  AppTimeLimitsWhitelistPolicyBuilder();
  ~AppTimeLimitsWhitelistPolicyBuilder();

  AppTimeLimitsWhitelistPolicyBuilder(
      const AppTimeLimitsWhitelistPolicyBuilder&) = delete;
  AppTimeLimitsWhitelistPolicyBuilder& operator=(
      const AppTimeLimitsWhitelistPolicyBuilder&) = delete;

  void SetUp();
  void Clear();
  void AppendToWhitelistUrlList(const std::string& scheme);
  void AppendToWhitelistAppList(const AppId& app_id);

  const base::Value& value() const { return value_; }

 private:
  void AppendToList(const std::string& key, base::Value value);

  base::Value value_;
};

}  // namespace app_time
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_LIMITS_WHITELIST_POLICY_TEST_UTILS_H_
