// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/policy/minimum_version_policy_handler.h"
#include "chrome/browser/chromeos/policy/minimum_version_policy_handler_delegate_impl.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/update_required_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {
const char kNewVersion[] = "99999.4.2";
const int kNoWarning = 0;
const int kShortWarningInDays = 2;
const int kLongWarningInDays = 10;
constexpr base::TimeDelta kShortWarning =
    base::TimeDelta::FromDays(kShortWarningInDays);
constexpr base::TimeDelta kLongWarning =
    base::TimeDelta::FromDays(kLongWarningInDays);
const char kPublicSessionId[] = "demo@example.com";
// This is a randomly chosen long delay in milliseconds to make sure that the
// timer keeps running for a long time in case it is started.
const int kAutoLoginLoginDelayMilliseconds = 500000;

policy::MinimumVersionPolicyHandler* GetMinimumVersionPolicyHandler() {
  return g_browser_process->platform_part()
      ->browser_policy_connector_chromeos()
      ->GetMinimumVersionPolicyHandler();
}

}  //  namespace

class MinimumVersionPolicyTestBase : public chromeos::LoginManagerTest {
 public:
  MinimumVersionPolicyTestBase() = default;

  ~MinimumVersionPolicyTestBase() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    LoginManagerTest::SetUpInProcessBrowserTestFixture();
    auto fake_update_engine_client =
        std::make_unique<chromeos::FakeUpdateEngineClient>();
    fake_update_engine_client_ = fake_update_engine_client.get();
    chromeos::DBusThreadManager::GetSetterForTesting()->SetUpdateEngineClient(
        std::move(fake_update_engine_client));
  }

  // Set new value for policy and wait till setting is changed.
  void SetDevicePolicyAndWaitForSettingChange(const base::Value& value);

  // Set new value for policy.
  void SetAndRefreshMinimumChromeVersionPolicy(const base::Value& value);

  // Create a new requirement as a dictionary to be used in the policy value.
  base::Value CreateRequirement(const std::string& version,
                                int warning,
                                int eol_warning) const;

 protected:
  void SetMinimumChromeVersionPolicy(const base::Value& value);

  DevicePolicyCrosTestHelper helper_;
  chromeos::FakeUpdateEngineClient* fake_update_engine_client_ = nullptr;
  chromeos::DeviceStateMixin device_state_{
      &mixin_host_,
      chromeos::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

void MinimumVersionPolicyTestBase::SetMinimumChromeVersionPolicy(
    const base::Value& value) {
  policy::DevicePolicyBuilder* const device_policy(helper_.device_policy());
  em::ChromeDeviceSettingsProto& proto(device_policy->payload());
  std::string policy_value;
  EXPECT_TRUE(base::JSONWriter::Write(value, &policy_value));
  proto.mutable_minimum_chrome_version_enforced()->set_value(policy_value);
}

void MinimumVersionPolicyTestBase::SetDevicePolicyAndWaitForSettingChange(
    const base::Value& value) {
  SetMinimumChromeVersionPolicy(value);
  helper_.RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
      {chromeos::kMinimumChromeVersionEnforced});
}

void MinimumVersionPolicyTestBase::SetAndRefreshMinimumChromeVersionPolicy(
    const base::Value& value) {
  SetMinimumChromeVersionPolicy(value);
  helper_.RefreshDevicePolicy();
}

// Create a dictionary value to represent minimum version requirement.
// |version| - a string containing the minimum required version.
// |warning| - number of days representing the warning period.
// |eol_warning| - number of days representing the end of life warning period.
base::Value MinimumVersionPolicyTestBase::CreateRequirement(
    const std::string& version,
    const int warning,
    const int eol_warning) const {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey(MinimumVersionPolicyHandler::kChromeVersion, version);
  dict.SetIntKey(MinimumVersionPolicyHandler::kWarningPeriod, warning);
  dict.SetIntKey(MinimumVersionPolicyHandler::KEolWarningPeriod, eol_warning);
  return dict;
}

class MinimumVersionPolicyTest : public MinimumVersionPolicyTestBase {
 public:
  MinimumVersionPolicyTest() { login_manager_.AppendRegularUsers(1); }
  ~MinimumVersionPolicyTest() override = default;

  void Login();
  void MarkUserManaged();

 protected:
  chromeos::LoginManagerMixin login_manager_{&mixin_host_};
};

void MinimumVersionPolicyTest::Login() {
  const auto& users = login_manager_.users();
  EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 0u);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);

  LoginUser(users[0].account_id);
  EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 1u);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);
}

void MinimumVersionPolicyTest::MarkUserManaged() {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  DCHECK(profile);
  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
}

IN_PROC_BROWSER_TEST_F(MinimumVersionPolicyTest, CriticalUpdateOnLoginScreen) {
  EXPECT_EQ(ash::LoginScreenTestApi::GetUsersCount(), 1);
  EXPECT_FALSE(ash::LoginScreenTestApi::IsOobeDialogVisible());

  // Create policy value as a list of requirements.
  base::Value requirement_list(base::Value::Type::LIST);
  base::Value new_version_no_warning =
      CreateRequirement(kNewVersion, kNoWarning, kNoWarning);
  requirement_list.Append(std::move(new_version_no_warning));

  // Set new value for policy and check update required screen is shown on the
  // login screen.
  SetDevicePolicyAndWaitForSettingChange(requirement_list);
  chromeos::OobeScreenWaiter(chromeos::UpdateRequiredView::kScreenId).Wait();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());

  // Revoke policy and check update required screen is hidden.
  base::Value empty_list(base::Value::Type::LIST);
  SetDevicePolicyAndWaitForSettingChange(empty_list);
  chromeos::OobeScreenExitWaiter(chromeos::UpdateRequiredView::kScreenId)
      .Wait();
  EXPECT_FALSE(ash::LoginScreenTestApi::IsOobeDialogVisible());
}

IN_PROC_BROWSER_TEST_F(MinimumVersionPolicyTest, PRE_CriticalUpdateInSession) {
  // Login the user into the session and mark as managed.
  Login();
  MarkUserManaged();

  // Create policy value as a list of requirements.
  base::Value requirement_list(base::Value::Type::LIST);
  base::Value new_version_no_warning =
      CreateRequirement(kNewVersion, kNoWarning, kNoWarning);
  requirement_list.Append(std::move(new_version_no_warning));

  // Create waiter to observe termination notification.
  content::WindowedNotificationObserver termination_waiter(
      chrome::NOTIFICATION_APP_TERMINATING,
      content::NotificationService::AllSources());

  // Set new value for policy and check that user is logged out of the session.
  SetDevicePolicyAndWaitForSettingChange(requirement_list);
  termination_waiter.Wait();
  EXPECT_TRUE(chrome::IsAttemptingShutdown());
}

IN_PROC_BROWSER_TEST_F(MinimumVersionPolicyTest, CriticalUpdateInSession) {
  // Check login screen is shown post chrome restart due to critical update
  // required in session.
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);
  EXPECT_EQ(ash::LoginScreenTestApi::GetUsersCount(), 1);
  // TODO(https://crbug.com/1048607): Show update required screen after user is
  // logged out of session due to critical update required by policy.
  EXPECT_FALSE(ash::LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 0u);
}

IN_PROC_BROWSER_TEST_F(MinimumVersionPolicyTest, NonCriticalUpdateGoodNetwork) {
  // Login the user into the session and mark as managed.
  Login();

  // Check deadline timer is not running and local state is not set.
  PrefService* prefs = g_browser_process->local_state();
  base::Time timer_start_time =
      prefs->GetTime(prefs::kUpdateRequiredTimerStartTime);
  EXPECT_TRUE(timer_start_time.is_null());
  EXPECT_FALSE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());

  // Create and set policy value with short warning time.
  base::Value requirement_short_warning(base::Value::Type::LIST);
  requirement_short_warning.Append(
      CreateRequirement(kNewVersion, kShortWarningInDays, kShortWarningInDays));
  SetDevicePolicyAndWaitForSettingChange(requirement_short_warning);

  // Policy handler sets the local state and starts the deadline timer.
  timer_start_time = prefs->GetTime(prefs::kUpdateRequiredTimerStartTime);
  EXPECT_FALSE(timer_start_time.is_null());
  EXPECT_EQ(prefs->GetTimeDelta(prefs::kUpdateRequiredWarningPeriod),
            kShortWarning);
  EXPECT_TRUE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());

  // Create and set policy value with long warning time.
  base::Value requirement_long_warning(base::Value::Type::LIST);
  requirement_long_warning.Append(
      CreateRequirement(kNewVersion, kLongWarningInDays, kLongWarningInDays));
  SetDevicePolicyAndWaitForSettingChange(requirement_long_warning);

  // Warning time is increased but timer start time does not change.
  EXPECT_EQ(prefs->GetTime(prefs::kUpdateRequiredTimerStartTime),
            timer_start_time);
  EXPECT_EQ(prefs->GetTimeDelta(prefs::kUpdateRequiredWarningPeriod),
            kLongWarning);

  // Create and set policy value with no warning time.
  base::Value requirement_no_warning(base::Value::Type::LIST);
  requirement_no_warning.Append(
      CreateRequirement(kNewVersion, kNoWarning, kNoWarning));
  SetDevicePolicyAndWaitForSettingChange(requirement_no_warning);

  // Warning time is not reduced as policy does not allow to reduce deadline.
  EXPECT_EQ(prefs->GetTime(prefs::kUpdateRequiredTimerStartTime),
            timer_start_time);
  EXPECT_EQ(prefs->GetTimeDelta(prefs::kUpdateRequiredWarningPeriod),
            kLongWarning);

  EXPECT_TRUE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->GetState());

  // Simulate update installed from update_engine_client and check that timer
  // and the local state is reset.
  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::UPDATED_NEED_REBOOT);
  status.set_new_version("99999.9");
  fake_update_engine_client_->set_default_status(status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  EXPECT_FALSE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());
  EXPECT_FALSE(GetMinimumVersionPolicyHandler()->GetState());
  EXPECT_TRUE(prefs->GetTime(prefs::kUpdateRequiredTimerStartTime).is_null());
}

IN_PROC_BROWSER_TEST_F(MinimumVersionPolicyTest,
                       CriticalUpdateInSessionUnmanagedUser) {
  // Login the user into the session.
  Login();

  // Create policy value as a list of requirements.
  base::Value requirement_list(base::Value::Type::LIST);
  base::Value new_version_no_warning =
      CreateRequirement(kNewVersion, kNoWarning, kNoWarning);
  requirement_list.Append(std::move(new_version_no_warning));

  // Set new value for pref and check that user session is not terminated.
  SetDevicePolicyAndWaitForSettingChange(requirement_list);
  EXPECT_FALSE(chrome::IsAttemptingShutdown());
}

class MinimumVersionNoUsersLoginTest : public MinimumVersionPolicyTestBase {
 public:
  MinimumVersionNoUsersLoginTest() = default;
  ~MinimumVersionNoUsersLoginTest() override = default;

 protected:
  chromeos::LoginManagerMixin login_manager_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(MinimumVersionNoUsersLoginTest,
                       CriticalUpdateOnLoginScreen) {
  chromeos::OobeScreenWaiter(chromeos::GaiaView::kScreenId).Wait();
  EXPECT_EQ(ash::LoginScreenTestApi::GetUsersCount(), 0);

  // Create policy value as a list of requirements.
  base::Value requirement_list(base::Value::Type::LIST);
  base::Value new_version_no_warning =
      CreateRequirement(kNewVersion, kNoWarning, kNoWarning);
  requirement_list.Append(std::move(new_version_no_warning));

  // Set new value for policy and check update required screen is shown on the
  // login screen.
  SetDevicePolicyAndWaitForSettingChange(requirement_list);
  chromeos::OobeScreenWaiter(chromeos::UpdateRequiredView::kScreenId).Wait();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());

  // Revoke policy and check update required screen is hidden and gaia screen is
  // shown.
  base::Value empty_list(base::Value::Type::LIST);
  SetDevicePolicyAndWaitForSettingChange(empty_list);
  chromeos::OobeScreenExitWaiter(chromeos::UpdateRequiredView::kScreenId)
      .Wait();
  chromeos::OobeScreenWaiter(chromeos::GaiaView::kScreenId).Wait();
}

class MinimumVersionPolicyPresentTest : public MinimumVersionPolicyTestBase {
 public:
  MinimumVersionPolicyPresentTest() {}
  ~MinimumVersionPolicyPresentTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    MinimumVersionPolicyTestBase::SetUpInProcessBrowserTestFixture();

    // Create policy value as a list of requirements.
    base::Value requirement_list(base::Value::Type::LIST);
    base::Value new_version_no_warning =
        CreateRequirement(kNewVersion, kNoWarning, kNoWarning);
    requirement_list.Append(std::move(new_version_no_warning));

    // Set new policy value.
    SetAndRefreshMinimumChromeVersionPolicy(requirement_list);
  }
};

IN_PROC_BROWSER_TEST_F(MinimumVersionPolicyPresentTest,
                       DeadlineReachedNoUsers) {
  // Checks update required screen is shown at startup if there is no user in
  // the device.
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);
  chromeos::OobeScreenWaiter(chromeos::UpdateRequiredView::kScreenId).Wait();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
}

class MinimumVersionExistingUserTest : public MinimumVersionPolicyPresentTest {
 public:
  MinimumVersionExistingUserTest() {
    // Start with user pods.
    login_mixin_.AppendManagedUsers(1);
  }

 protected:
  chromeos::LoginManagerMixin login_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(MinimumVersionExistingUserTest, DeadlineReached) {
  // Checks update required screen is shown at startup if user is existing in
  // the device.
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);
  chromeos::OobeScreenWaiter(chromeos::UpdateRequiredView::kScreenId).Wait();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
}

class MinimumVersionBeforeLoginHost : public MinimumVersionExistingUserTest {
 public:
  MinimumVersionBeforeLoginHost() {}
  ~MinimumVersionBeforeLoginHost() override = default;

  bool SetUpUserDataDirectory() override {
    // LoginManagerMixin sets up command line in the SetUpUserDataDirectory.
    if (!MinimumVersionPolicyTestBase::SetUpUserDataDirectory())
      return false;
    // Postpone login host creation.
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        chromeos::switches::kForceLoginManagerInTests);
    return true;
  }
};

IN_PROC_BROWSER_TEST_F(MinimumVersionBeforeLoginHost, DeadlineReached) {
  // Checks update required screen is shown at startup if the policy handler is
  // invoked before login display host is created.
  EXPECT_EQ(chromeos::LoginDisplayHost::default_host(), nullptr);
  EXPECT_TRUE(GetMinimumVersionPolicyHandler());
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->DeadlineReached());
  ShowLoginWizard(chromeos::OobeScreen::SCREEN_UNKNOWN);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);
  chromeos::OobeScreenWaiter(chromeos::UpdateRequiredView::kScreenId).Wait();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
}

class MinimumVersionPublicSessionAutoLoginTest
    : public MinimumVersionExistingUserTest {
 public:
  MinimumVersionPublicSessionAutoLoginTest() {}
  ~MinimumVersionPublicSessionAutoLoginTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    MinimumVersionExistingUserTest::SetUpInProcessBrowserTestFixture();
    AddPublicSessionToDevicePolicy(kPublicSessionId);
  }

  void AddPublicSessionToDevicePolicy(const std::string& user) {
    em::ChromeDeviceSettingsProto& proto(helper_.device_policy()->payload());
    DeviceLocalAccountTestHelper::AddPublicSession(&proto, user);
    helper_.RefreshDevicePolicy();
    em::DeviceLocalAccountsProto* device_local_accounts =
        proto.mutable_device_local_accounts();
    device_local_accounts->set_auto_login_id(user);
    device_local_accounts->set_auto_login_delay(
        kAutoLoginLoginDelayMilliseconds);
    helper_.RefreshDevicePolicy();
  }
};

IN_PROC_BROWSER_TEST_F(MinimumVersionPublicSessionAutoLoginTest,
                       BlockAutoLogin) {
  // Checks public session auto login is blocked if update is required on
  // reboot.
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);
  chromeos::OobeScreenWaiter(chromeos::UpdateRequiredView::kScreenId).Wait();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_FALSE(chromeos::ExistingUserController::current_controller()
                   ->IsSigninInProgress());
  EXPECT_FALSE(chromeos::ExistingUserController::current_controller()
                   ->IsAutoLoginTimerRunningForTesting());
}

class MinimumVersionTimerExpiredOnLogin
    : public MinimumVersionPolicyTestBase,
      public chromeos::LocalStateMixin::Delegate {
 public:
  MinimumVersionTimerExpiredOnLogin() = default;
  ~MinimumVersionTimerExpiredOnLogin() override = default;

  // chromeos::LocalStateMixin::Delegate:
  void SetUpLocalState() override {
    // Set up local state to reflect that update required deadline has passed
    // when device is rebooted.
    const base::TimeDelta delta = base::TimeDelta::FromDays(5);
    PrefService* prefs = g_browser_process->local_state();
    prefs->SetTime(prefs::kUpdateRequiredTimerStartTime,
                   base::Time::Now() - delta);
    prefs->SetTimeDelta(prefs::kUpdateRequiredWarningPeriod, kShortWarning);
  }

  // MinimumVersionPolicyTestBase:
  void SetUpInProcessBrowserTestFixture() override {
    MinimumVersionPolicyTestBase::SetUpInProcessBrowserTestFixture();
    // Create and set policy value as a list of requirements.
    base::Value requirement_list(base::Value::Type::LIST);
    requirement_list.Append(CreateRequirement(kNewVersion, kShortWarningInDays,
                                              kShortWarningInDays));
    SetAndRefreshMinimumChromeVersionPolicy(requirement_list);
  }

 private:
  chromeos::LocalStateMixin local_state_mixin_{&mixin_host_, this};
};

IN_PROC_BROWSER_TEST_F(MinimumVersionTimerExpiredOnLogin, DeadlinePassed) {
  // Show update required screen as deadline to update the device has passed.
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);
  chromeos::OobeScreenWaiter(chromeos::UpdateRequiredView::kScreenId).Wait();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
}

}  // namespace policy
