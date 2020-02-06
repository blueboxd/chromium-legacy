// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_secure_dns_handler.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/dns_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif

using testing::_;
using testing::Return;

namespace settings {

class TestSecureDnsHandler : public SecureDnsHandler {
 public:
  // Pull WebUIMessageHandler::set_web_ui() into public so tests can call it.
  using SecureDnsHandler::set_web_ui;
};

class SecureDnsHandlerTest : public InProcessBrowserTest {
 protected:
#if defined(OS_WIN)
  SecureDnsHandlerTest()
      // Mark as not enterprise managed to prevent the secure DNS mode from
      // being downgraded to off.
      : scoped_domain_(false) {}
#else
  SecureDnsHandlerTest() = default;
#endif
  ~SecureDnsHandlerTest() override = default;

  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    // Initialize user policy.
    ON_CALL(provider_, IsInitializationComplete(_)).WillByDefault(Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void SetUpOnMainThread() override {
    handler_ = std::make_unique<TestSecureDnsHandler>();
    handler_->set_web_ui(&web_ui_);
    handler_->RegisterMessages();
    handler_->AllowJavascriptForTesting();
    base::RunLoop().RunUntilIdle();
  }

  void TearDownOnMainThread() override { handler_.reset(); }

  // Updates out-params from the last message sent to WebUI about a secure DNS
  // change. False is returned if the message was invalid or
  // not found.
  bool GetLastSettingsChangedMessage(std::string* secure_dns_mode) {
    for (auto it = web_ui_.call_data().rbegin();
         it != web_ui_.call_data().rend(); ++it) {
      const content::TestWebUI::CallData* data = it->get();
      if (data->function_name() != "cr.webUIListenerCallback" ||
          !data->arg1()->is_string() ||
          data->arg1()->GetString() != "secure-dns-setting-changed") {
        continue;
      }

      const base::DictionaryValue* dict = nullptr;
      if (!data->arg2()->GetAsDictionary(&dict))
        return false;

      if (!dict->FindStringPath("mode"))
        return false;
      *secure_dns_mode = *dict->FindStringPath("mode");

      return true;
    }
    return false;
  }

  // Sets a policy update which will cause power pref managed change.
  void SetPolicyForPolicyKey(policy::PolicyMap* policy_map,
                             const std::string& policy_key,
                             std::unique_ptr<base::Value> value) {
    policy_map->Set(policy_key, policy::POLICY_LEVEL_MANDATORY,
                    policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                    std::move(value), nullptr);
    provider_.UpdateChromePolicy(*policy_map);
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<TestSecureDnsHandler> handler_;
  content::TestWebUI web_ui_;
  policy::MockConfigurationPolicyProvider provider_;

 private:
#if defined(OS_WIN)
  base::win::ScopedDomainStateForTesting scoped_domain_;
#endif

  DISALLOW_COPY_AND_ASSIGN(SecureDnsHandlerTest);
};

IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, SecureDnsModes) {
  PrefService* local_state = g_browser_process->local_state();
  std::string secure_dns_mode;

  local_state->SetString(prefs::kDnsOverHttpsMode,
                         chrome_browser_net::kDnsOverHttpsModeOff);
  EXPECT_TRUE(GetLastSettingsChangedMessage(&secure_dns_mode));
  EXPECT_EQ(chrome_browser_net::kDnsOverHttpsModeOff, secure_dns_mode);

  local_state->SetString(prefs::kDnsOverHttpsMode,
                         chrome_browser_net::kDnsOverHttpsModeAutomatic);
  EXPECT_TRUE(GetLastSettingsChangedMessage(&secure_dns_mode));
  EXPECT_EQ(chrome_browser_net::kDnsOverHttpsModeAutomatic, secure_dns_mode);

  local_state->SetString(prefs::kDnsOverHttpsMode,
                         chrome_browser_net::kDnsOverHttpsModeSecure);
  EXPECT_TRUE(GetLastSettingsChangedMessage(&secure_dns_mode));
  EXPECT_EQ(chrome_browser_net::kDnsOverHttpsModeSecure, secure_dns_mode);

  local_state->SetString(prefs::kDnsOverHttpsMode, "unknown");
  EXPECT_TRUE(GetLastSettingsChangedMessage(&secure_dns_mode));
  EXPECT_EQ(chrome_browser_net::kDnsOverHttpsModeOff, secure_dns_mode);
}

IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, SecureDnsPolicy) {
  policy::PolicyMap policy_map;
  SetPolicyForPolicyKey(&policy_map, policy::key::kDnsOverHttpsMode,
                        std::make_unique<base::Value>(
                            chrome_browser_net::kDnsOverHttpsModeAutomatic));

  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kDnsOverHttpsMode,
                         chrome_browser_net::kDnsOverHttpsModeSecure);

  std::string secure_dns_mode;
  EXPECT_TRUE(GetLastSettingsChangedMessage(&secure_dns_mode));
  EXPECT_EQ(chrome_browser_net::kDnsOverHttpsModeAutomatic, secure_dns_mode);
}

IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, SecureDnsPolicyChange) {
  policy::PolicyMap policy_map;
  SetPolicyForPolicyKey(&policy_map, policy::key::kDnsOverHttpsMode,
                        std::make_unique<base::Value>(
                            chrome_browser_net::kDnsOverHttpsModeAutomatic));

  std::string secure_dns_mode;
  EXPECT_TRUE(GetLastSettingsChangedMessage(&secure_dns_mode));
  EXPECT_EQ(chrome_browser_net::kDnsOverHttpsModeAutomatic, secure_dns_mode);

  SetPolicyForPolicyKey(
      &policy_map, policy::key::kDnsOverHttpsMode,
      std::make_unique<base::Value>(chrome_browser_net::kDnsOverHttpsModeOff));
  EXPECT_TRUE(GetLastSettingsChangedMessage(&secure_dns_mode));
  EXPECT_EQ(chrome_browser_net::kDnsOverHttpsModeOff, secure_dns_mode);
}

// On platforms where enterprise policies do not have default values, test
// that DoH is disabled when non-DoH policies are set.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, OtherPoliciesSet) {
  policy::PolicyMap policy_map;
  SetPolicyForPolicyKey(&policy_map, policy::key::kIncognitoModeAvailability,
                        std::make_unique<base::Value>(1));

  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kDnsOverHttpsMode,
                         chrome_browser_net::kDnsOverHttpsModeSecure);

  std::string secure_dns_mode;
  EXPECT_TRUE(GetLastSettingsChangedMessage(&secure_dns_mode));
  EXPECT_EQ(chrome_browser_net::kDnsOverHttpsModeOff, secure_dns_mode);
}
#endif

}  // namespace settings
