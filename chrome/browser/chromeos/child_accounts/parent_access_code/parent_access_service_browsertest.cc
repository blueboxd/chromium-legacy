// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/child_accounts/child_account_test_utils.h"
#include "chrome/browser/chromeos/child_accounts/parent_access_code/config_source.h"
#include "chrome/browser/chromeos/child_accounts/parent_access_code/parent_access_service.h"
#include "chrome/browser/chromeos/child_accounts/parent_access_code/parent_access_test_utils.h"
#include "chrome/browser/chromeos/policy/login_policy_test_base.h"
#include "chrome/browser/chromeos/policy/user_policy_test_helper.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace parent_access {

namespace {

// Dictionary keys for ParentAccessCodeConfig policy.
constexpr char kFutureConfigDictKey[] = "future_config";
constexpr char kCurrentConfigDictKey[] = "current_config";
constexpr char kOldConfigsDictKey[] = "old_configs";

base::DictionaryValue PolicyFromConfigs(
    const AccessCodeConfig& future_config,
    const AccessCodeConfig& current_config,
    const std::vector<AccessCodeConfig>& old_configs) {
  base::DictionaryValue dict;
  dict.SetKey(kFutureConfigDictKey, future_config.ToDictionary());
  dict.SetKey(kCurrentConfigDictKey, current_config.ToDictionary());
  base::Value old_configs_value(base::Value::Type::LIST);
  for (const auto& config : old_configs)
    old_configs_value.Append(config.ToDictionary());
  dict.SetKey(kOldConfigsDictKey, std::move(old_configs_value));
  return dict;
}

}  // namespace

// Stores information about results of the access code validation.
struct CodeValidationResults {
  // Number of successful access code validations.
  int success_count = 0;

  // Number of attempts when access code validation failed.
  int failure_count = 0;
};

// ParentAccessServiceObserver implementation used for tests.
class TestParentAccessServiceObserver : public ParentAccessService::Observer {
 public:
  explicit TestParentAccessServiceObserver(const AccountId& account_id)
      : account_id_(account_id) {}
  ~TestParentAccessServiceObserver() override = default;

  void OnAccessCodeValidation(bool result,
                              base::Optional<AccountId> account_id) override {
    ASSERT_TRUE(account_id);
    EXPECT_EQ(account_id_, account_id.value());
    result ? ++validation_results_.success_count
           : ++validation_results_.failure_count;
  }

  CodeValidationResults validation_results_;

 private:
  const AccountId account_id_;

  DISALLOW_COPY_AND_ASSIGN(TestParentAccessServiceObserver);
};

class ParentAccessServiceTest : public policy::LoginPolicyTestBase {
 public:
  ParentAccessServiceTest()
      : test_observer_(
            std::make_unique<TestParentAccessServiceObserver>(child_)) {}
  ~ParentAccessServiceTest() override = default;

  void SetUp() override {
    // Recognize example.com (used by LoginPolicyTestBase) as non-enterprise
    // account.
    policy::BrowserPolicyConnector::SetNonEnterpriseDomainForTesting(
        "example.com");

    policy::LoginPolicyTestBase::SetUp();
  }

  void SetUpOnMainThread() override {
    ASSERT_NO_FATAL_FAILURE(GetTestAccessCodeValues(&test_values_));
    ParentAccessService::Get().AddObserver(test_observer_.get());
    policy::LoginPolicyTestBase::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    ParentAccessService::Get().RemoveObserver(test_observer_.get());
    policy::LoginPolicyTestBase::TearDownOnMainThread();
  }

  std::string GetIdToken() const override {
    return test::GetChildAccountOAuthIdToken();
  }

 protected:
  void LogInChild() {
    SkipToLoginScreen();
    LogIn(kAccountId, kAccountPassword, test::kChildAccountServiceFlags);
  }

  // Updates the policy containing the Parent Access Code config.
  void UpdatePolicy(const base::DictionaryValue& dict) {
    const user_manager::UserManager* const user_manager =
        user_manager::UserManager::Get();
    EXPECT_TRUE(user_manager->GetActiveUser()->IsChild());
    Profile* child_profile =
        ProfileHelper::Get()->GetProfileByUser(user_manager->GetActiveUser());

    std::string config_string;
    base::JSONWriter::Write(dict, &config_string);

    base::DictionaryValue policy;
    policy.SetKey(policy::key::kParentAccessCodeConfig,
                  base::Value(config_string));
    user_policy_helper()->SetPolicyAndWait(
        std::move(policy), base::DictionaryValue(), child_profile);
  }

  // Performs |code| validation on ParentAccessService singleton using the
  // |validation time| and returns the result.
  bool ValidateAccessCode(const std::string& code, base::Time validation_time) {
    return ParentAccessService::Get().ValidateParentAccessCode(child_, code,
                                                               validation_time);
  }

  // Checks if ParentAccessServiceObserver and ValidateParentAccessCodeCallback
  // were called as intended. Expects |success_count| of successful access code
  // validations and |failure_count| of failed validation attempts.
  void ExpectResults(int success_count, int failure_count) {
    EXPECT_EQ(success_count, test_observer_->validation_results_.success_count);
    EXPECT_EQ(failure_count, test_observer_->validation_results_.failure_count);
  }

  const AccountId child_ = AccountId::FromUserEmail(kAccountId);
  AccessCodeValues test_values_;
  std::unique_ptr<TestParentAccessServiceObserver> test_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ParentAccessServiceTest);
};

IN_PROC_BROWSER_TEST_F(ParentAccessServiceTest, NoConfigAvailable) {
  LogInChild();

  auto test_value = test_values_.begin();
  EXPECT_FALSE(ValidateAccessCode(test_value->second, test_value->first));

  ExpectResults(0, 1);
}

IN_PROC_BROWSER_TEST_F(ParentAccessServiceTest, NoValidConfigAvailable) {
  LogInChild();

  std::vector<AccessCodeConfig> old_configs;
  old_configs.emplace_back(GetInvalidTestConfig());
  UpdatePolicy(PolicyFromConfigs(GetInvalidTestConfig(), GetInvalidTestConfig(),
                                 old_configs));

  auto test_value = test_values_.begin();
  EXPECT_FALSE(ValidateAccessCode(test_value->second, test_value->first));

  ExpectResults(0, 1);
}

IN_PROC_BROWSER_TEST_F(ParentAccessServiceTest, ValidationWithFutureConfig) {
  LogInChild();

  std::vector<AccessCodeConfig> old_configs;
  old_configs.emplace_back(GetInvalidTestConfig());
  UpdatePolicy(PolicyFromConfigs(GetDefaultTestConfig(), GetInvalidTestConfig(),
                                 old_configs));

  auto test_value = test_values_.begin();
  EXPECT_TRUE(ValidateAccessCode(test_value->second, test_value->first));

  ExpectResults(1, 0);
}

IN_PROC_BROWSER_TEST_F(ParentAccessServiceTest, ValidationWithCurrentConfig) {
  LogInChild();

  std::vector<AccessCodeConfig> old_configs;
  old_configs.emplace_back(GetInvalidTestConfig());
  UpdatePolicy(PolicyFromConfigs(GetInvalidTestConfig(), GetDefaultTestConfig(),
                                 old_configs));

  auto test_value = test_values_.begin();
  EXPECT_TRUE(ValidateAccessCode(test_value->second, test_value->first));

  ExpectResults(1, 0);
}

IN_PROC_BROWSER_TEST_F(ParentAccessServiceTest, ValidationWithOldConfig) {
  LogInChild();

  std::vector<AccessCodeConfig> old_configs;
  old_configs.emplace_back(GetInvalidTestConfig());
  old_configs.emplace_back(GetDefaultTestConfig());
  UpdatePolicy(PolicyFromConfigs(GetInvalidTestConfig(), GetInvalidTestConfig(),
                                 old_configs));

  auto test_value = test_values_.begin();
  EXPECT_TRUE(ValidateAccessCode(test_value->second, test_value->first));

  ExpectResults(1, 0);
}

IN_PROC_BROWSER_TEST_F(ParentAccessServiceTest, MultipleValidationAttempts) {
  LogInChild();

  AccessCodeValues::iterator test_value = test_values_.begin();

  // No config - validation should fail.
  EXPECT_FALSE(ValidateAccessCode(test_value->second, test_value->first));

  UpdatePolicy(
      PolicyFromConfigs(GetInvalidTestConfig(), GetDefaultTestConfig(), {}));

  // Valid config - validation should pass.
  for (auto& value : test_values_)
    EXPECT_TRUE(ValidateAccessCode(value.second, value.first));

  UpdatePolicy(
      PolicyFromConfigs(GetInvalidTestConfig(), GetInvalidTestConfig(), {}));

  // Invalid config - validation should fail.
  EXPECT_FALSE(ValidateAccessCode(test_value->second, test_value->first));

  ExpectResults(test_values_.size(), 2);
}

IN_PROC_BROWSER_TEST_F(ParentAccessServiceTest, NoObserver) {
  LogInChild();

  ParentAccessService::Get().RemoveObserver(test_observer_.get());

  UpdatePolicy(
      PolicyFromConfigs(GetInvalidTestConfig(), GetDefaultTestConfig(), {}));

  auto test_value = test_values_.begin();
  EXPECT_TRUE(ValidateAccessCode(test_value->second, test_value->first));

  ExpectResults(0, 0);
}

IN_PROC_BROWSER_TEST_F(ParentAccessServiceTest, NoAccountId) {
  LogInChild();

  ParentAccessService::Get().RemoveObserver(test_observer_.get());

  UpdatePolicy(
      PolicyFromConfigs(GetInvalidTestConfig(), GetDefaultTestConfig(), {}));

  auto test_value = test_values_.begin();

  EXPECT_TRUE(ParentAccessService::Get().ValidateParentAccessCode(
      EmptyAccountId(), test_value->second, test_value->first));
}

IN_PROC_BROWSER_TEST_F(ParentAccessServiceTest, InvalidAccountId) {
  LogInChild();

  ParentAccessService::Get().RemoveObserver(test_observer_.get());

  UpdatePolicy(
      PolicyFromConfigs(GetInvalidTestConfig(), GetDefaultTestConfig(), {}));

  auto test_value = test_values_.begin();

  AccountId other_child = AccountId::FromUserEmail("otherchild@gmail.com");
  EXPECT_FALSE(ParentAccessService::Get().ValidateParentAccessCode(
      other_child, test_value->second, test_value->first));
}

}  // namespace parent_access
}  // namespace chromeos
