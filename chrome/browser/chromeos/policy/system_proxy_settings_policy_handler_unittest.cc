// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/system_proxy_settings_policy_handler.h"

#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class SystemProxySettingsPolicyHandlerTest : public testing::Test {
 protected:
  SystemProxySettingsPolicyHandlerTest() = default;
  ~SystemProxySettingsPolicyHandlerTest() override = default;

  // testing::Test
  void SetUp() override { testing::Test::SetUp(); }

  void TearDown() override {}

 protected:
  void SetPolicy(bool system_proxy_enabled,
                 const std::string& system_services_username,
                 const std::string& system_services_password) {
    base::DictionaryValue dict;
    dict.SetKey("system_proxy_enabled", base::Value(system_proxy_enabled));
    dict.SetKey("system_services_username",
                base::Value(system_services_username));
    dict.SetKey("system_services_password",
                base::Value(system_services_password));
    scoped_testing_cros_settings_.device_settings()->Set(
        chromeos::kSystemProxySettings, dict);
  }

  base::test::TaskEnvironment task_environment_;
  chromeos::ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

// Verifies that System-proxy is started and shutdown according to the
// |kSystemProxySettings| policy.
TEST_F(SystemProxySettingsPolicyHandlerTest, SystemProxyStartShutdown) {
  SystemProxySettingsPolicyHandler proxy_policy_handler(
      chromeos::CrosSettings::Get());

  // TODO(acostinas, chromium:1042626) Add test logic when the dbus client for
  // System-proxy is implemented.
  SetPolicy(true /* system_proxy_enabled */, "" /* system_services_username */,
            "" /* system_services_password */);
  SetPolicy(false /* system_proxy_enabled */, "" /* system_services_username */,
            "" /* system_services_password */);
}

}  // namespace policy
