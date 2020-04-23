// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/minimum_version_policy_handler.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "chromeos/settings/cros_settings_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;

using MinimumVersionRequirement =
    policy::MinimumVersionPolicyHandler::MinimumVersionRequirement;

namespace policy {

namespace {
const char kFakeCurrentVersion[] = "80.25.4";
const char kNewVersion[] = "81.4.2";
const char kNewerVersion[] = "81.5.4";
const char kNewestVersion[] = "82";
const char kOldVersion[] = "78.1.5";

const int kLongWarning = 10;
const int kShortWarning = 2;
const int kNoWarning = 0;

}  // namespace

class MinimumVersionPolicyHandlerTest
    : public testing::Test,
      public MinimumVersionPolicyHandler::Delegate {
 public:
  MinimumVersionPolicyHandlerTest();

  void SetUp() override;
  void TearDown() override;

  // MinimumVersionPolicyHandler::Delegate:
  bool IsKioskMode() const;
  bool IsEnterpriseManaged() const;
  const base::Version& GetCurrentVersion() const;
  bool IsUserManaged() const;
  bool IsUserLoggedIn() const;
  bool IsLoginInProgress() const;
  MOCK_METHOD0(ShowUpdateRequiredScreen, void());
  MOCK_METHOD0(RestartToLoginScreen, void());
  MOCK_METHOD0(HideUpdateRequiredScreenIfShown, void());
  MOCK_CONST_METHOD0(IsLoginSessionState, bool());

  void SetCurrentVersionString(std::string version);

  void CreateMinimumVersionHandler();
  const MinimumVersionPolicyHandler::MinimumVersionRequirement* GetState()
      const;

  // Set new value for policy pref.
  void SetPolicyPref(base::Value value);

  // Create a new requirement as a dictionary to be used in the policy value.
  base::Value CreateRequirement(const std::string& version,
                                int warning,
                                int eol_warning) const;

  MinimumVersionPolicyHandler* GetMinimumVersionPolicyHandler() {
    return minimum_version_policy_handler_.get();
  }

  void SetUserManaged(bool managed) { user_managed_ = managed; }

 private:
  bool user_managed_ = true;

  base::test::TaskEnvironment task_environment;
  chromeos::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  std::unique_ptr<base::Version> current_version_;
  std::unique_ptr<MinimumVersionPolicyHandler> minimum_version_policy_handler_;
};

MinimumVersionPolicyHandlerTest::MinimumVersionPolicyHandlerTest() {}

void MinimumVersionPolicyHandlerTest::SetUp() {
  auto fake_update_engine_client =
      std::make_unique<chromeos::FakeUpdateEngineClient>();
  chromeos::DBusThreadManager::GetSetterForTesting()->SetUpdateEngineClient(
      std::move(fake_update_engine_client));

  CreateMinimumVersionHandler();
  SetCurrentVersionString(kFakeCurrentVersion);
}

void MinimumVersionPolicyHandlerTest::TearDown() {}

void MinimumVersionPolicyHandlerTest::CreateMinimumVersionHandler() {
  minimum_version_policy_handler_.reset(
      new MinimumVersionPolicyHandler(this, chromeos::CrosSettings::Get()));
}

const MinimumVersionRequirement* MinimumVersionPolicyHandlerTest::GetState()
    const {
  return minimum_version_policy_handler_->GetState();
}

void MinimumVersionPolicyHandlerTest::SetCurrentVersionString(
    std::string version) {
  current_version_.reset(new base::Version(version));
  ASSERT_TRUE(current_version_->IsValid());
}

bool MinimumVersionPolicyHandlerTest::IsKioskMode() const {
  return false;
}

bool MinimumVersionPolicyHandlerTest::IsEnterpriseManaged() const {
  return true;
}

bool MinimumVersionPolicyHandlerTest::IsUserManaged() const {
  return user_managed_;
}

bool MinimumVersionPolicyHandlerTest::IsUserLoggedIn() const {
  return true;
}

bool MinimumVersionPolicyHandlerTest::IsLoginInProgress() const {
  return false;
}

const base::Version& MinimumVersionPolicyHandlerTest::GetCurrentVersion()
    const {
  return *current_version_;
}

void MinimumVersionPolicyHandlerTest::SetPolicyPref(base::Value value) {
  scoped_testing_cros_settings_.device_settings()->Set(
      chromeos::kMinimumChromeVersionEnforced, value);
}

/**
 *  Create a dictionary value to represent minimum version requirement.
 *  @param version The minimum required version in string form.
 *  @param warning The warning period in days.
 *  @param eol_warning The end of life warning period in days.
 */
base::Value MinimumVersionPolicyHandlerTest::CreateRequirement(
    const std::string& version,
    const int warning,
    const int eol_warning) const {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey(MinimumVersionPolicyHandler::kChromeVersion, version);
  dict.SetIntKey(MinimumVersionPolicyHandler::kWarningPeriod, warning);
  dict.SetIntKey(MinimumVersionPolicyHandler::KEolWarningPeriod, eol_warning);
  return dict;
}

TEST_F(MinimumVersionPolicyHandlerTest, RequirementsNotMetState) {
  // No policy applied yet. Check requirements are satisfied.
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());
  EXPECT_FALSE(GetState());

  // Create policy value as a list of requirements.
  base::Value requirement_list(base::Value::Type::LIST);

  base::Value new_version_short_warning =
      CreateRequirement(kNewVersion, kShortWarning, kNoWarning);
  auto strongest_requirement = MinimumVersionRequirement::CreateInstanceIfValid(
      &base::Value::AsDictionaryValue(new_version_short_warning));
  base::Value newer_version_long_warning =
      CreateRequirement(kNewerVersion, kLongWarning, kNoWarning);
  base::Value newest_version_no_warning =
      CreateRequirement(kNewestVersion, kNoWarning, kNoWarning);

  requirement_list.Append(std::move(new_version_short_warning));
  requirement_list.Append(std::move(newer_version_long_warning));
  requirement_list.Append(std::move(newest_version_no_warning));

  // Set new value for pref and check that requirements are not satisfied.
  // The state in |MinimumVersionPolicyHandler| should be equal to the strongest
  // requirement as defined in the policy description.
  SetPolicyPref(std::move(requirement_list));

  EXPECT_FALSE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());
  EXPECT_TRUE(GetState());
  EXPECT_TRUE(strongest_requirement);
  EXPECT_EQ(GetState()->Compare(strongest_requirement.get()), 0);

  // Reset the pref to empty list and verify state is reset.
  base::Value requirement_list2(base::Value::Type::LIST);
  SetPolicyPref(std::move(requirement_list2));
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());
  EXPECT_FALSE(GetState());
}

TEST_F(MinimumVersionPolicyHandlerTest, CriticalUpdates) {
  // No policy applied yet. Check requirements are satisfied.
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());
  EXPECT_FALSE(GetState());

  base::RunLoop run_loop;
  // Expect calls to make sure that user is logged out.
  EXPECT_CALL(*this, RestartToLoginScreen())
      .Times(1)
      .WillOnce(testing::Invoke([&run_loop]() {
        run_loop.Quit();
        return false;
      }));
  EXPECT_CALL(*this, ShowUpdateRequiredScreen()).Times(0);
  EXPECT_CALL(*this, HideUpdateRequiredScreenIfShown()).Times(0);
  EXPECT_CALL(*this, IsLoginSessionState())
      .Times(1)
      .WillOnce(testing::Return(false));

  // Create policy value as a list of requirements.
  base::Value requirement_list(base::Value::Type::LIST);
  base::Value new_version_no_warning =
      CreateRequirement(kNewVersion, kNoWarning, kLongWarning);
  base::Value newer_version_long_warning =
      CreateRequirement(kNewerVersion, kLongWarning, kNoWarning);
  requirement_list.Append(std::move(new_version_no_warning));
  requirement_list.Append(std::move(newer_version_long_warning));

  // Set new value for pref and check that requirements are not satisfied.
  // As the warning time is set to zero, the user should be logged out of the
  // session.
  SetPolicyPref(std::move(requirement_list));
  // Start the run loop to wait for EOL status fetch.
  run_loop.Run();
  EXPECT_FALSE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());
  EXPECT_TRUE(GetState());
}

TEST_F(MinimumVersionPolicyHandlerTest, CriticalUpdatesUnmanagedUser) {
  // No policy applied yet. Check requirements are satisfied.
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());
  EXPECT_FALSE(GetState());

  base::RunLoop run_loop;
  // Expect calls to make sure that user is not logged out.
  EXPECT_CALL(*this, RestartToLoginScreen()).Times(0);
  EXPECT_CALL(*this, ShowUpdateRequiredScreen()).Times(0);
  EXPECT_CALL(*this, HideUpdateRequiredScreenIfShown()).Times(0);
  // Unmanaged user is not logged out of the session. The run loop is quit on
  // reaching IsLoginSessionState() because that implies we have fetched the
  // EOL status and reached the end of the policy handler code flow.
  EXPECT_CALL(*this, IsLoginSessionState())
      .Times(1)
      .WillOnce(testing::Invoke([&run_loop]() {
        run_loop.Quit();
        return false;
      }));

  // Set user as unmanaged.
  SetUserManaged(false);

  // Create policy value as a list of requirements.
  base::Value requirement_list(base::Value::Type::LIST);
  base::Value new_version_no_warning =
      CreateRequirement(kNewVersion, kNoWarning, kLongWarning);
  requirement_list.Append(std::move(new_version_no_warning));

  // Set new value for pref and check that requirements are not satisfied.
  // Unmanaged user should not be logged out of the session.
  SetPolicyPref(std::move(requirement_list));
  // Start the run loop to wait for EOL status fetch.
  run_loop.Run();
  EXPECT_FALSE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());
  EXPECT_TRUE(GetState());
}

TEST_F(MinimumVersionPolicyHandlerTest, RequirementsMetState) {
  // No policy applied yet. Check requirements are satisfied.
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());
  EXPECT_FALSE(GetState());

  // Create policy value as a list of requirements.
  base::Value requirement_list(base::Value::Type::LIST);
  base::Value current_version_no_warning =
      CreateRequirement(kFakeCurrentVersion, kNoWarning, kNoWarning);
  base::Value old_version_long_warning =
      CreateRequirement(kOldVersion, kLongWarning, kNoWarning);
  requirement_list.Append(std::move(current_version_no_warning));
  requirement_list.Append(std::move(old_version_long_warning));

  // Set new value for pref and check that requirements are still satisfied
  // as none of the requirements has version greater than current version.
  SetPolicyPref(std::move(requirement_list));
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());
  EXPECT_FALSE(GetState());
}

}  // namespace policy
