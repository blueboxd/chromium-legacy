// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/system_proxy_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/system_proxy/system_proxy_client.h"
#include "chromeos/dbus/system_proxy/system_proxy_service.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::WithArg;

namespace {
constexpr char kSystemServicesUsername[] = "test_username";
constexpr char kSystemServicesPassword[] = "test_password";
constexpr char kKerberosActivePrincipalName[] = "kerberos_princ_name";
constexpr char kProxyAuthUrl[] = "http://example.com:3128";
constexpr char kRealm[] = "My proxy";
constexpr char kScheme[] = "BaSiC";
}  // namespace

namespace policy {
class SystemProxyManagerTest : public testing::Test {
 public:
  SystemProxyManagerTest() : local_state_(TestingBrowserProcess::GetGlobal()) {}
  ~SystemProxyManagerTest() override = default;

  // testing::Test
  void SetUp() override {
    testing::Test::SetUp();
    profile_ = std::make_unique<TestingProfile>();
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

  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState local_state_;
  std::unique_ptr<TestingProfile> profile_;
  chromeos::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  chromeos::ScopedDeviceSettingsTestHelper device_settings_test_helper_;
  chromeos::ScopedStubInstallAttributes test_install_attributes_;
};

// Verifies that System-proxy is configured with the system traffic credentials
// set by |kSystemProxySettings| policy.
TEST_F(SystemProxyManagerTest, SetAuthenticationDetails) {
  SystemProxyManager system_proxy_manager(chromeos::CrosSettings::Get(),
                                          local_state_.Get());
  EXPECT_EQ(0, client_test_interface()->GetSetAuthenticationDetailsCallCount());

  SetPolicy(true /* system_proxy_enabled */, "" /* system_services_username */,
            "" /* system_services_password */);
  task_environment_.RunUntilIdle();
  // Don't send empty credentials.
  EXPECT_EQ(1, client_test_interface()->GetSetAuthenticationDetailsCallCount());

  SetPolicy(true /* system_proxy_enabled */, kSystemServicesUsername,
            kSystemServicesPassword);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(2, client_test_interface()->GetSetAuthenticationDetailsCallCount());

  system_proxy::SetAuthenticationDetailsRequest request =
      client_test_interface()->GetLastAuthenticationDetailsRequest();

  ASSERT_TRUE(request.has_credentials());
  EXPECT_EQ(kSystemServicesUsername, request.credentials().username());
  EXPECT_EQ(kSystemServicesPassword, request.credentials().password());
}

// Verifies requests to shut down are sent to System-proxy according to the
// |kSystemProxySettings| policy.
TEST_F(SystemProxyManagerTest, ShutDownDaemon) {
  SystemProxyManager system_proxy_manager(chromeos::CrosSettings::Get(),
                                          local_state_.Get());

  EXPECT_EQ(0, client_test_interface()->GetShutDownCallCount());

  SetPolicy(false /* system_proxy_enabled */, "" /* system_services_username */,
            "" /* system_services_password */);
  task_environment_.RunUntilIdle();
  // Don't send empty credentials.
  EXPECT_EQ(1, client_test_interface()->GetShutDownCallCount());
}

// Tests that |SystemProxyManager| sends the correct Kerberos details and
// updates to System-proxy.
TEST_F(SystemProxyManagerTest, KerberosConfig) {
  SystemProxyManager system_proxy_manager(chromeos::CrosSettings::Get(),
                                          local_state_.Get());

  SetPolicy(true /* system_proxy_enabled */, "" /* system_services_username */,
            "" /* system_services_password */);
  task_environment_.RunUntilIdle();
  local_state_.Get()->SetBoolean(prefs::kKerberosEnabled, true);
  EXPECT_EQ(2, client_test_interface()->GetSetAuthenticationDetailsCallCount());

  // Listen for pref changes for the primary profile.
  system_proxy_manager.StartObservingPrimaryProfilePrefs(profile_.get());
  EXPECT_EQ(3, client_test_interface()->GetSetAuthenticationDetailsCallCount());
  system_proxy::SetAuthenticationDetailsRequest request =
      client_test_interface()->GetLastAuthenticationDetailsRequest();
  EXPECT_FALSE(request.has_credentials());
  EXPECT_TRUE(request.kerberos_enabled());

  // Set an active principal name.
  profile_->GetPrefs()->SetString(prefs::kKerberosActivePrincipalName,
                                  kKerberosActivePrincipalName);
  EXPECT_EQ(4, client_test_interface()->GetSetAuthenticationDetailsCallCount());
  request = client_test_interface()->GetLastAuthenticationDetailsRequest();
  EXPECT_EQ(kKerberosActivePrincipalName, request.active_principal_name());

  // Remove the active principal name.
  profile_->GetPrefs()->SetString(prefs::kKerberosActivePrincipalName, "");
  request = client_test_interface()->GetLastAuthenticationDetailsRequest();
  EXPECT_EQ("", request.active_principal_name());
  EXPECT_TRUE(request.kerberos_enabled());

  // Disable kerberos.
  local_state_.Get()->SetBoolean(prefs::kKerberosEnabled, false);
  request = client_test_interface()->GetLastAuthenticationDetailsRequest();
  EXPECT_FALSE(request.kerberos_enabled());

  system_proxy_manager.StopObservingPrimaryProfilePrefs();
}

// Tests that when no user is signed in, credential requests are resolved to a
// d-bus call which sends back to System-proxy empty credentials for the
// specified protection space.
TEST_F(SystemProxyManagerTest, UserCredentialsRequiredNoUser) {
  SystemProxyManager system_proxy_manager(chromeos::CrosSettings::Get(),
                                          local_state_.Get());
  SetPolicy(true /* system_proxy_enabled */, "" /* system_services_username */,
            "" /* system_services_password */);

  system_proxy::ProtectionSpace protection_space;
  protection_space.set_origin(kProxyAuthUrl);
  protection_space.set_scheme(kScheme);
  protection_space.set_realm(kRealm);

  system_proxy::AuthenticationRequiredDetails details;
  *details.mutable_proxy_protection_space() = protection_space;

  client_test_interface()->SendAuthenticationRequiredSignal(details);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(2, client_test_interface()->GetSetAuthenticationDetailsCallCount());

  system_proxy::SetAuthenticationDetailsRequest request =
      client_test_interface()->GetLastAuthenticationDetailsRequest();

  ASSERT_TRUE(request.has_protection_space());
  ASSERT_EQ(protection_space.SerializeAsString(),
            request.protection_space().SerializeAsString());

  ASSERT_TRUE(request.has_credentials());
  EXPECT_EQ("", request.credentials().username());
  EXPECT_EQ("", request.credentials().password());
}
}  // namespace policy
