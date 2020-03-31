// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/login_screen_test_api.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

IN_PROC_BROWSER_TEST_F(OobeBaseTest, PRE_InterruptedAutoStartEnrollment) {
  StartupUtils::MarkOobeCompleted();
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kDeviceEnrollmentAutoStart, true);
  prefs->SetBoolean(prefs::kDeviceEnrollmentCanExit, false);
}

// Tests that the default first screen is the welcome screen after OOBE
// when auto enrollment is enabled and device is not yet enrolled.
IN_PROC_BROWSER_TEST_F(OobeBaseTest, InterruptedAutoStartEnrollment) {
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
  LoginUITestBase()
      : LoginManagerTest(false, false /* should_initialize_webui */) {
    set_force_webui_login(false);
  }

 protected:
  std::vector<LoginManagerMixin::TestUserInfo> test_users_{
      LoginManagerMixin::CreateRegularUsers(10)};
  LoginManagerMixin login_manager_mixin_{&mixin_host_, test_users_};
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
  LoginManagerMixin::TestUserInfo owner_{test_users_[3]};
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED};
  ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

// Verifies basic login UI properties.
IN_PROC_BROWSER_TEST_F(LoginUIConsumerTest, LoginUIVisible) {
  const int users_count = test_users_.size();
  EXPECT_EQ(users_count, ash::LoginScreenTestApi::GetUsersCount());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsOobeDialogVisible());

  for (int i = 0; i < users_count; ++i) {
    EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(test_users_[i].account_id));
  }

  for (int i = 0; i < users_count; ++i) {
    // Can't remove the owner.
    EXPECT_EQ(ash::LoginScreenTestApi::RemoveUser(test_users_[i].account_id),
              test_users_[i].account_id != owner_.account_id);
  }

  EXPECT_EQ(1, ash::LoginScreenTestApi::GetUsersCount());
  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(owner_.account_id));
  EXPECT_FALSE(ash::LoginScreenTestApi::IsOobeDialogVisible());
}

IN_PROC_BROWSER_TEST_F(LoginUIEnrolledTest, UserRemoval) {
  const int users_count = test_users_.size();
  EXPECT_EQ(users_count, ash::LoginScreenTestApi::GetUsersCount());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsOobeDialogVisible());

  // Remove the first user.
  EXPECT_TRUE(ash::LoginScreenTestApi::RemoveUser(test_users_[0].account_id));
  EXPECT_EQ(users_count - 1, ash::LoginScreenTestApi::GetUsersCount());

  // Can not remove twice.
  EXPECT_FALSE(ash::LoginScreenTestApi::RemoveUser(test_users_[0].account_id));
  EXPECT_EQ(users_count - 1, ash::LoginScreenTestApi::GetUsersCount());

  for (int i = 1; i < users_count; ++i) {
    EXPECT_TRUE(ash::LoginScreenTestApi::RemoveUser(test_users_[i].account_id));
    EXPECT_EQ(users_count - i - 1, ash::LoginScreenTestApi::GetUsersCount());
  }

  // Gaia dialog should be shown again as there are no users anymore.
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
}

IN_PROC_BROWSER_TEST_F(LoginUIEnrolledTest, UserReverseRemoval) {
  const int users_count = test_users_.size();
  EXPECT_EQ(users_count, ash::LoginScreenTestApi::GetUsersCount());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsOobeDialogVisible());

  for (int i = users_count - 1; i >= 0; --i) {
    EXPECT_TRUE(ash::LoginScreenTestApi::RemoveUser(test_users_[i].account_id));
    EXPECT_EQ(i, ash::LoginScreenTestApi::GetUsersCount());
  }

  // Gaia dialog should be shown again as there are no users anymore.
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
}

}  // namespace chromeos
