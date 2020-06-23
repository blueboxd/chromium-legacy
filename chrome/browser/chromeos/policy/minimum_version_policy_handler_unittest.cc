// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/minimum_version_policy_handler.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

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
const char kUpdateRequiredNotificationId[] = "policy.update_required";
const char kCellularServicePath[] = "/service/cellular1";

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

  NotificationDisplayServiceTester* display_service() {
    return notification_service_.get();
  }

  chromeos::FakeUpdateEngineClient* update_engine() {
    return fake_update_engine_client_;
  }

  void SetUserManaged(bool managed) { user_managed_ = managed; }

  content::BrowserTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  bool user_managed_ = true;
  ScopedTestingLocalState local_state_;
  base::test::ScopedFeatureList feature_list_;
  chromeos::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  std::unique_ptr<NotificationDisplayServiceTester> notification_service_;
  chromeos::ScopedStubInstallAttributes scoped_stub_install_attributes_;
  chromeos::FakeUpdateEngineClient* fake_update_engine_client_;
  std::unique_ptr<base::Version> current_version_;
  std::unique_ptr<MinimumVersionPolicyHandler> minimum_version_policy_handler_;
};

MinimumVersionPolicyHandlerTest::MinimumVersionPolicyHandlerTest()
    : local_state_(TestingBrowserProcess::GetGlobal()) {
  feature_list_.InitAndEnableFeature(chromeos::features::kMinimumChromeVersion);
}

void MinimumVersionPolicyHandlerTest::SetUp() {
  auto fake_update_engine_client =
      std::make_unique<chromeos::FakeUpdateEngineClient>();
  fake_update_engine_client_ = fake_update_engine_client.get();
  chromeos::DBusThreadManager::GetSetterForTesting()->SetUpdateEngineClient(
      std::move(fake_update_engine_client));
  chromeos::NetworkHandler::Initialize();

  chromeos::ShillServiceClient::TestInterface* service_test =
      chromeos::DBusThreadManager::Get()
          ->GetShillServiceClient()
          ->GetTestInterface();
  service_test->ClearServices();
  service_test->AddService("/service/eth", "eth" /* guid */, "eth",
                           shill::kTypeEthernet, shill::kStateOnline,
                           true /* visible */);
  base::RunLoop().RunUntilIdle();

  scoped_stub_install_attributes_.Get()->SetCloudManaged("managed.com",
                                                         "device_id");
  TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
      std::make_unique<SystemNotificationHelper>());
  notification_service_ =
      std::make_unique<NotificationDisplayServiceTester>(nullptr /*profile*/);

  CreateMinimumVersionHandler();
  SetCurrentVersionString(kFakeCurrentVersion);
}

void MinimumVersionPolicyHandlerTest::TearDown() {
  minimum_version_policy_handler_.reset();
  chromeos::NetworkHandler::Shutdown();
}

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

TEST_F(MinimumVersionPolicyHandlerTest, DeadlineTimerExpired) {
  // Checks the user is logged out of the session when the deadline is reached.
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());

  // This is needed to wait till EOL status is fetched from the update_engine.
  base::RunLoop run_loop;
  GetMinimumVersionPolicyHandler()->set_fetch_eol_callback_for_testing(
      run_loop.QuitClosure());

  // Expect calls to make sure that user is not logged out.
  EXPECT_CALL(*this, RestartToLoginScreen()).Times(0);
  EXPECT_CALL(*this, ShowUpdateRequiredScreen()).Times(0);

  // Create and set pref value to invoke policy handler such that update is
  // required with a long warning time.
  base::Value requirement_list(base::Value::Type::LIST);
  requirement_list.Append(
      CreateRequirement(kNewVersion, kLongWarning, kLongWarning));
  SetPolicyPref(std::move(requirement_list));

  run_loop.Run();
  EXPECT_TRUE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());
  EXPECT_FALSE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());

  testing::Mock::VerifyAndClearExpectations(this);

  // Expire the timer and check that user is logged out of the session.
  EXPECT_CALL(*this, IsLoginSessionState()).Times(1);
  EXPECT_CALL(*this, RestartToLoginScreen()).Times(1);
  const base::TimeDelta warning = base::TimeDelta::FromDays(kLongWarning);
  task_environment.FastForwardBy(warning);
  EXPECT_FALSE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());
  EXPECT_FALSE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());
}

TEST_F(MinimumVersionPolicyHandlerTest, NoNetworkNotifications) {
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());

  // Disconnect all networks
  chromeos::ShillServiceClient::TestInterface* service_test =
      chromeos::DBusThreadManager::Get()
          ->GetShillServiceClient()
          ->GetTestInterface();
  service_test->ClearServices();

  // This is needed to wait till EOL status is fetched from the update_engine.
  base::RunLoop run_loop;
  GetMinimumVersionPolicyHandler()->set_fetch_eol_callback_for_testing(
      run_loop.QuitClosure());

  // Create and set pref value to invoke policy handler.
  base::Value requirement_list(base::Value::Type::LIST);
  requirement_list.Append(
      CreateRequirement(kNewVersion, kLongWarning, kLongWarning));
  SetPolicyPref(std::move(requirement_list));

  run_loop.Run();
  EXPECT_TRUE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());
  EXPECT_FALSE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());

  // Check notification is shown for offline devices with the warning time.
  base::string16 expected_title =
      base::ASCIIToUTF16("Update device within 10 days");
  base::string16 expected_message = base::ASCIIToUTF16(
      "managed.com requires you to download an update before the deadline. The "
      "update will download automatically when you connect to the internet.");
  auto notification_long_waiting =
      display_service()->GetNotification(kUpdateRequiredNotificationId);
  ASSERT_TRUE(notification_long_waiting);
  EXPECT_EQ(notification_long_waiting->title(), expected_title);
  EXPECT_EQ(notification_long_waiting->message(), expected_message);

  // Expire the notification timer to show new notification on the last day.
  const base::TimeDelta warning = base::TimeDelta::FromDays(kLongWarning - 1);
  task_environment.FastForwardBy(warning);

  base::string16 expected_title_last_day =
      base::ASCIIToUTF16("Last day to update device");
  base::string16 expected_message_last_day = base::ASCIIToUTF16(
      "managed.com requires you to download an update today. The "
      "update will download automatically when you connect to the internet.");
  auto notification_last_day =
      display_service()->GetNotification(kUpdateRequiredNotificationId);
  ASSERT_TRUE(notification_long_waiting);
  EXPECT_EQ(notification_last_day->title(), expected_title_last_day);
  EXPECT_EQ(notification_last_day->message(), expected_message_last_day);
}

TEST_F(MinimumVersionPolicyHandlerTest, MeteredNetworkNotifications) {
  // Connect to metered network
  chromeos::ShillServiceClient::TestInterface* service_test =
      chromeos::DBusThreadManager::Get()
          ->GetShillServiceClient()
          ->GetTestInterface();
  service_test->ClearServices();
  service_test->AddService(kCellularServicePath,
                           kCellularServicePath /* guid */,
                           kCellularServicePath, shill::kTypeCellular,
                           shill::kStateOnline, true /* visible */);
  base::RunLoop().RunUntilIdle();

  // This is needed to wait till EOL status is fetched from the update_engine.
  base::RunLoop run_loop;
  GetMinimumVersionPolicyHandler()->set_fetch_eol_callback_for_testing(
      run_loop.QuitClosure());

  // Create and set pref value to invoke policy handler.
  base::Value requirement_list(base::Value::Type::LIST);
  requirement_list.Append(
      CreateRequirement(kNewVersion, kLongWarning, kLongWarning));
  SetPolicyPref(std::move(requirement_list));
  run_loop.Run();
  EXPECT_TRUE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());

  // Check notification is shown for metered network with the warning time.
  base::string16 expected_title =
      base::ASCIIToUTF16("Update device within 10 days");
  base::string16 expected_message = base::ASCIIToUTF16(
      "managed.com requires you to connect to Wi-Fi and download an update "
      "before the deadline. Or, download from a metered connection (charges "
      "may apply).");
  auto notification_long_waiting =
      display_service()->GetNotification(kUpdateRequiredNotificationId);
  ASSERT_TRUE(notification_long_waiting);
  EXPECT_EQ(notification_long_waiting->title(), expected_title);
  EXPECT_EQ(notification_long_waiting->message(), expected_message);

  // Expire the notification timer to show new notification on the last day.
  const base::TimeDelta warning = base::TimeDelta::FromDays(kLongWarning - 1);
  task_environment.FastForwardBy(warning);

  base::string16 expected_title_last_day =
      base::ASCIIToUTF16("Last day to update device");
  base::string16 expected_message_last_day = base::ASCIIToUTF16(
      "managed.com requires you to connect to Wi-Fi today to download an "
      "update. Or, download from a metered connection (charges may apply).");
  auto notification_last_day =
      display_service()->GetNotification(kUpdateRequiredNotificationId);
  ASSERT_TRUE(notification_long_waiting);
  EXPECT_EQ(notification_last_day->title(), expected_title_last_day);
  EXPECT_EQ(notification_last_day->message(), expected_message_last_day);
}

TEST_F(MinimumVersionPolicyHandlerTest, EolNotifications) {
  // Set device state to end of life.
  update_engine()->set_eol_date(base::DefaultClock::GetInstance()->Now() -
                                base::TimeDelta::FromDays(1));

  // This is needed to wait till EOL status is fetched from the update_engine.
  base::RunLoop run_loop;
  GetMinimumVersionPolicyHandler()->set_fetch_eol_callback_for_testing(
      run_loop.QuitClosure());

  // Create and set pref value to invoke policy handler.
  base::Value requirement_list(base::Value::Type::LIST);
  requirement_list.Append(
      CreateRequirement(kNewVersion, kLongWarning, kLongWarning));
  SetPolicyPref(std::move(requirement_list));
  run_loop.Run();
  EXPECT_TRUE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());

  // Check notification is shown for end of life with the warning time.
  base::string16 expected_title =
      base::ASCIIToUTF16("Return device within 10 days");
  base::string16 expected_message = base::ASCIIToUTF16(
      "managed.com requires you to back up your data and return this device "
      "before the deadline.");
  auto notification_long_waiting =
      display_service()->GetNotification(kUpdateRequiredNotificationId);
  ASSERT_TRUE(notification_long_waiting);
  EXPECT_EQ(notification_long_waiting->title(), expected_title);
  EXPECT_EQ(notification_long_waiting->message(), expected_message);

  // Expire notification timer to show new notification a week before deadline.
  const base::TimeDelta warning = base::TimeDelta::FromDays(kLongWarning - 7);
  task_environment.FastForwardBy(warning);

  base::string16 expected_title_one_week =
      base::ASCIIToUTF16("Return device within 7 days");
  auto notification_one_week =
      display_service()->GetNotification(kUpdateRequiredNotificationId);
  ASSERT_TRUE(notification_one_week);
  EXPECT_EQ(notification_one_week->title(), expected_title_one_week);
  EXPECT_EQ(notification_one_week->message(), expected_message);

  // Expire the notification timer to show new notification on the last day.
  const base::TimeDelta warning_last_day = base::TimeDelta::FromDays(6);
  task_environment.FastForwardBy(warning_last_day);

  base::string16 expected_title_last_day =
      base::ASCIIToUTF16("Immediate return required");
  base::string16 expected_message_last_day = base::ASCIIToUTF16(
      "managed.com requires you to back up your data and return this device "
      "today.");
  auto notification_last_day =
      display_service()->GetNotification(kUpdateRequiredNotificationId);
  ASSERT_TRUE(notification_long_waiting);
  EXPECT_EQ(notification_last_day->title(), expected_title_last_day);
  EXPECT_EQ(notification_last_day->message(), expected_message_last_day);
}

}  // namespace policy
