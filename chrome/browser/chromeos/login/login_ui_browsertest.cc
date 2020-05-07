// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/login_screen_test_api.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/lock/screen_locker_tester.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/screens/user_selection_screen.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/local_state_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/scoped_policy_update.h"
#include "chrome/browser/chromeos/login/test/user_policy_mixin.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "content/public/test/browser_test.h"

namespace chromeos {

class InterruptedAutoStartEnrollmentTest : public OobeBaseTest,
                                           public LocalStateMixin::Delegate {
 public:
  InterruptedAutoStartEnrollmentTest() = default;
  ~InterruptedAutoStartEnrollmentTest() override = default;

  void SetUpLocalState() override {
    StartupUtils::MarkOobeCompleted();
    PrefService* prefs = g_browser_process->local_state();
    prefs->SetBoolean(::prefs::kDeviceEnrollmentAutoStart, true);
    prefs->SetBoolean(::prefs::kDeviceEnrollmentCanExit, false);
  }
};

// Tests that the default first screen is the welcome screen after OOBE
// when auto enrollment is enabled and device is not yet enrolled.
IN_PROC_BROWSER_TEST_F(InterruptedAutoStartEnrollmentTest, ShowsWelcome) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(OobeBaseTest, OobeNoExceptions) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  OobeBaseTest::CheckJsExceptionErrors(0);
}

IN_PROC_BROWSER_TEST_F(OobeBaseTest, OobeCatchException) {
  OobeBaseTest::CheckJsExceptionErrors(0);
  test::OobeJS().ExecuteAsync("aelrt('misprint')");
  OobeBaseTest::CheckJsExceptionErrors(1);
  test::OobeJS().ExecuteAsync("consle.error('Some error')");
  OobeBaseTest::CheckJsExceptionErrors(2);
}

class LoginUITestBase : public LoginManagerTest {
 public:
  LoginUITestBase() : LoginManagerTest() {
    login_manager_mixin_.AppendRegularUsers(10);
  }

 protected:
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

class LoginUIEnrolledTest : public LoginUITestBase {
 public:
  LoginUIEnrolledTest() = default;
  ~LoginUIEnrolledTest() override = default;

 protected:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

class LoginUIConsumerTest : public LoginUITestBase {
 public:
  LoginUIConsumerTest() = default;
  ~LoginUIConsumerTest() override = default;

  void SetUpOnMainThread() override {
    scoped_testing_cros_settings_.device_settings()->Set(
        kDeviceOwner, base::Value(owner_.account_id.GetUserEmail()));
    LoginUITestBase::SetUpOnMainThread();
  }

 protected:
  LoginManagerMixin::TestUserInfo owner_{login_manager_mixin_.users()[3]};
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED};
  ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

// Verifies basic login UI properties.
IN_PROC_BROWSER_TEST_F(LoginUIConsumerTest, LoginUIVisible) {
  const auto& test_users = login_manager_mixin_.users();
  const int users_count = test_users.size();
  EXPECT_EQ(users_count, ash::LoginScreenTestApi::GetUsersCount());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsOobeDialogVisible());

  for (int i = 0; i < users_count; ++i) {
    EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(test_users[i].account_id));
  }

  for (int i = 0; i < users_count; ++i) {
    // Can't remove the owner.
    EXPECT_EQ(ash::LoginScreenTestApi::RemoveUser(test_users[i].account_id),
              test_users[i].account_id != owner_.account_id);
  }

  EXPECT_EQ(1, ash::LoginScreenTestApi::GetUsersCount());
  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(owner_.account_id));
  EXPECT_FALSE(ash::LoginScreenTestApi::IsOobeDialogVisible());
}

IN_PROC_BROWSER_TEST_F(LoginUIEnrolledTest, UserRemoval) {
  const auto& test_users = login_manager_mixin_.users();
  const int users_count = test_users.size();
  EXPECT_EQ(users_count, ash::LoginScreenTestApi::GetUsersCount());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsOobeDialogVisible());

  // Remove the first user.
  EXPECT_TRUE(ash::LoginScreenTestApi::RemoveUser(test_users[0].account_id));
  EXPECT_EQ(users_count - 1, ash::LoginScreenTestApi::GetUsersCount());

  // Can not remove twice.
  EXPECT_FALSE(ash::LoginScreenTestApi::RemoveUser(test_users[0].account_id));
  EXPECT_EQ(users_count - 1, ash::LoginScreenTestApi::GetUsersCount());

  for (int i = 1; i < users_count; ++i) {
    EXPECT_TRUE(ash::LoginScreenTestApi::RemoveUser(test_users[i].account_id));
    EXPECT_EQ(users_count - i - 1, ash::LoginScreenTestApi::GetUsersCount());
  }

  // Gaia dialog should be shown again as there are no users anymore.
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
}

IN_PROC_BROWSER_TEST_F(LoginUIEnrolledTest, UserReverseRemoval) {
  const auto& test_users = login_manager_mixin_.users();
  const int users_count = test_users.size();
  EXPECT_EQ(users_count, ash::LoginScreenTestApi::GetUsersCount());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsOobeDialogVisible());

  for (int i = users_count - 1; i >= 0; --i) {
    EXPECT_TRUE(ash::LoginScreenTestApi::RemoveUser(test_users[i].account_id));
    EXPECT_EQ(i, ash::LoginScreenTestApi::GetUsersCount());
  }

  // Gaia dialog should be shown again as there are no users anymore.
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
}

class DisplayPasswordButtonTest : public LoginManagerTest {
 public:
  DisplayPasswordButtonTest() : LoginManagerTest() {}

  void SetDisplayPasswordButtonEnabledLoginAndLock(
      bool display_password_button_enabled) {
    // Sets the feature by user policy
    {
      std::unique_ptr<ScopedUserPolicyUpdate> scoped_user_policy_update =
          user_policy_mixin_.RequestPolicyUpdate();
      scoped_user_policy_update->policy_payload()
          ->mutable_logindisplaypasswordbuttonenabled()
          ->set_value(display_password_button_enabled);
    }

    chromeos::WizardController::SkipPostLoginScreensForTesting();

    auto context = LoginManagerMixin::CreateDefaultUserContext(test_user_);
    login_manager_mixin_.LoginAndWaitForActiveSession(context);

    ScreenLockerTester screen_locker_tester;
    screen_locker_tester.Lock();

    EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(test_user_.account_id));
  }

  void SetUpInProcessBrowserTestFixture() override {
    LoginManagerTest::SetUpInProcessBrowserTestFixture();
    // Login as a managed user would save force-online-signin to true and
    // invalidate the auth token into local state, which would prevent to focus
    // during the second part of the test which happens in the login screen.
    UserSelectionScreen::SetSkipForceOnlineSigninForTesting(true);
  }

 protected:
  const LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId("user@example.com", "1111")};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, test_user_.account_id};
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

// Check if the display password button feature is disabled on the lock screen
// after login into a session and locking the screen.
IN_PROC_BROWSER_TEST_F(DisplayPasswordButtonTest,
                       PRE_LoginUIDisplayPasswordButtonDisabled) {
  SetDisplayPasswordButtonEnabledLoginAndLock(false);
  EXPECT_FALSE(ash::LoginScreenTestApi::IsDisplayPasswordButtonShown(
      test_user_.account_id));
}

// Check if the display password button feature is disabled on the login screen.
IN_PROC_BROWSER_TEST_F(DisplayPasswordButtonTest,
                       LoginUIDisplayPasswordButtonDisabled) {
  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(test_user_.account_id));
  EXPECT_FALSE(ash::LoginScreenTestApi::IsDisplayPasswordButtonShown(
      test_user_.account_id));
}

// Check if the display password button feature is enabled on the lock screen
// after login into a session and locking the screen.
IN_PROC_BROWSER_TEST_F(DisplayPasswordButtonTest,
                       PRE_LoginUIDisplayPasswordButtonEnabled) {
  SetDisplayPasswordButtonEnabledLoginAndLock(true);
  EXPECT_TRUE(ash::LoginScreenTestApi::IsDisplayPasswordButtonShown(
      test_user_.account_id));
}

// Check if the display password button feature is enabled on the login screen.
IN_PROC_BROWSER_TEST_F(DisplayPasswordButtonTest,
                       LoginUIDisplayPasswordButtonEnabled) {
  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(test_user_.account_id));
  EXPECT_TRUE(ash::LoginScreenTestApi::IsDisplayPasswordButtonShown(
      test_user_.account_id));
}

}  // namespace chromeos
