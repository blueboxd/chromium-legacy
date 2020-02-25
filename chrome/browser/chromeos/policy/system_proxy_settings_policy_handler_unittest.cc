// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/system_proxy_settings_policy_handler.h"

#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chromeos/dbus/system_proxy/system_proxy_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
class SystemProxySettingsPolicyHandlerTest : public testing::Test {
 public:
  SystemProxySettingsPolicyHandlerTest() = default;
  ~SystemProxySettingsPolicyHandlerTest() override = default;

  // testing::Test
  void SetUp() override {
    testing::Test::SetUp();
    chromeos::SystemProxyClient::InitializeFake();
  }

  void TearDown() override { chromeos::SystemProxyClient::Shutdown(); }

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

  chromeos::SystemProxyClient::TestInterface* client_test_interface() {
    return chromeos::SystemProxyClient::Get()->GetTestInterface();
  }

  base::test::TaskEnvironment task_environment_;
  chromeos::ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

// Verifies that System-proxy is configured with the system traffic credentials
// set by |kSystemProxySettings| policy.
TEST_F(SystemProxySettingsPolicyHandlerTest, SetSystemTrafficCredentials) {
  SystemProxySettingsPolicyHandler proxy_policy_handler(
      chromeos::CrosSettings::Get());
  EXPECT_EQ(0,
            client_test_interface()->GetSetSystemTrafficCredentialsCallCount());

  SetPolicy(true /* system_proxy_enabled */, "" /* system_services_username */,
            "" /* system_services_password */);
  task_environment_.RunUntilIdle();
  // Don't send empty credentials.
  EXPECT_EQ(0,
            client_test_interface()->GetSetSystemTrafficCredentialsCallCount());

  SetPolicy(true /* system_proxy_enabled */,
            "test" /* system_services_username */,
            "test" /* system_services_password */);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1,
            client_test_interface()->GetSetSystemTrafficCredentialsCallCount());
}

// Verifies requests to shut down are sent to System-proxy according to the
// |kSystemProxySettings| policy.
TEST_F(SystemProxySettingsPolicyHandlerTest, ShutDownDaemon) {
  SystemProxySettingsPolicyHandler proxy_policy_handler(
      chromeos::CrosSettings::Get());

  EXPECT_EQ(0, client_test_interface()->GetShutDownCallCount());

  SetPolicy(false /* system_proxy_enabled */, "" /* system_services_username */,
            "" /* system_services_password */);
  task_environment_.RunUntilIdle();
  // Don't send empty credentials.
  EXPECT_EQ(1, client_test_interface()->GetShutDownCallCount());
}

}  // namespace policy
