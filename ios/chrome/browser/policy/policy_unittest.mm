// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/policy/browser_policy_connector_ios.h"
#include "ios/chrome/browser/policy/browser_state_policy_connector.h"
#include "ios/chrome/browser/policy/configuration_policy_handler_list_factory.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/prefs/browser_prefs.h"
#include "ios/chrome/browser/prefs/ios_chrome_pref_service_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class PolicyTest : public PlatformTest {
 public:
  PolicyTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableEnterprisePolicy);
  }

  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(state_directory_.CreateUniqueTempDir());

    // Create a BrowserPolicyConnectorIOS, install the mock policy
    // provider, and hook up Local State.
    browser_policy_connector_ = std::make_unique<BrowserPolicyConnectorIOS>(
        base::Bind(&BuildPolicyHandlerList));
    browser_policy_connector_->SetPolicyProviderForTesting(&policy_provider_);
    EXPECT_CALL(policy_provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));

    scoped_refptr<PrefRegistrySimple> local_state_registry(
        new PrefRegistrySimple);
    RegisterLocalStatePrefs(local_state_registry.get());
    local_state_ = CreateLocalState(
        state_directory_.GetPath().Append("TestLocalState"),
        base::ThreadTaskRunnerHandle::Get().get(), local_state_registry,
        browser_policy_connector_->GetPolicyService(),
        browser_policy_connector_.get());
    browser_policy_connector_->Init(local_state_.get(), nullptr);

    // Create a BrowserStatePolicyConnector and hook it up to prefs.
    browser_state_policy_connector_ =
        std::make_unique<BrowserStatePolicyConnector>();
    browser_state_policy_connector_->Init(
        browser_policy_connector_->GetSchemaRegistry(),
        browser_policy_connector_.get());
    scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry(
        new user_prefs::PrefRegistrySyncable);
    RegisterBrowserStatePrefs(pref_registry.get());
    pref_service_ = CreateBrowserStatePrefs(
        state_directory_.GetPath(), base::ThreadTaskRunnerHandle::Get().get(),
        pref_registry, browser_state_policy_connector_->GetPolicyService(),
        browser_policy_connector_.get());
  }

  void VerifyBooleanPolicy(const std::string& policyKey,
                           const std::string& prefPath) {
    // This preference is currently not managed
    EXPECT_FALSE(pref_service_->IsManagedPreference(prefPath));

    policy::PolicyMap values;
    // Setting the policy to true should set the pref to true.
    values.Set(policyKey, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
               std::make_unique<base::Value>(true), nullptr);
    policy_provider_.UpdateChromePolicy(values);
    EXPECT_TRUE(pref_service_->IsManagedPreference(prefPath));
    EXPECT_TRUE(pref_service_->GetBoolean(prefPath));

    // Setting the policy to false should set the pref to false.
    values.Set(policyKey, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
               std::make_unique<base::Value>(false), nullptr);
    policy_provider_.UpdateChromePolicy(values);
    EXPECT_TRUE(pref_service_->IsManagedPreference(prefPath));
    EXPECT_FALSE(pref_service_->GetBoolean(prefPath));
  }

 protected:
  // Temporary directory to hold preference files.
  base::ScopedTempDir state_directory_;

  // The task environment for this test.
  base::test::TaskEnvironment task_environment_;

  // Provides mock platform policy and can be modified during tests. Must
  // outlive |browser_policy_connector_|.
  policy::MockConfigurationPolicyProvider policy_provider_;

  // The application-level policy connector. Must outlive |local_state_|.
  std::unique_ptr<BrowserPolicyConnectorIOS> browser_policy_connector_;

  // The local state PrefService managed by policy.
  std::unique_ptr<PrefService> local_state_;

  // The BrowserState-level policy connector. Must outlive |pref_service_|.
  std::unique_ptr<BrowserStatePolicyConnector> browser_state_policy_connector_;

  // The PrefService managed by policy.
  std::unique_ptr<PrefService> pref_service_;
};

}  // namespace

// Tests that the SearchSuggestEnabled preference is correctly managed by
// policy.
TEST_F(PolicyTest, TestSearchSuggestEnabled) {
  VerifyBooleanPolicy(policy::key::kSearchSuggestEnabled,
                      prefs::kSearchSuggestEnabled);
}

// Tests that the PasswordManagerEnabled preference is correctly managed by
// policy.
TEST_F(PolicyTest, TestPasswordManagerEnabled) {
  VerifyBooleanPolicy(policy::key::kPasswordManagerEnabled,
                      password_manager::prefs::kCredentialsEnableService);
}
