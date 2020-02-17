// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "ui/compositor/compositor_switches.h"

namespace chromeos {

namespace {

struct {
  const char* email;
  const char* gaia_id;
} const kTestUsers[] = {{"test-user1@gmail.com", "1111111111"},
                        {"test-user2@gmail.com", "2222222222"}};

}  // namespace

class LoginUITest : public chromeos::LoginManagerTest {
 public:
  LoginUITest() : LoginManagerTest(false, true /* should_initialize_webui */) {
    for (size_t i = 0; i < base::size(kTestUsers); ++i) {
      test_users_.emplace_back(AccountId::FromUserEmailGaiaId(
          kTestUsers[i].email, kTestUsers[i].gaia_id));
    }
  }
  ~LoginUITest() override {}

 protected:
  std::vector<AccountId> test_users_;

  DISALLOW_COPY_AND_ASSIGN(LoginUITest);
};

IN_PROC_BROWSER_TEST_F(LoginUITest, PRE_LoginUIVisible) {
  RegisterUser(test_users_[0]);
  RegisterUser(test_users_[1]);
  StartupUtils::MarkOobeCompleted();
}

// Verifies basic login UI properties.
IN_PROC_BROWSER_TEST_F(LoginUITest, LoginUIVisible) {
  test::OobeJS().ExpectTrue("!!document.querySelector('#account-picker')");
  test::OobeJS().ExpectTrue("!!document.querySelector('#pod-row')");
  test::OobeJS().ExpectTrue(
      "document.querySelectorAll('.pod:not(#user-pod-template)').length == 2");

  test::OobeJS().ExpectTrue(
      "document.querySelectorAll('.pod:not(#user-pod-template)')[0]"
      ".user.emailAddress == '" +
      test_users_[0].GetUserEmail() + "'");
  test::OobeJS().ExpectTrue(
      "document.querySelectorAll('.pod:not(#user-pod-template)')[1]"
      ".user.emailAddress == '" +
      test_users_[1].GetUserEmail() + "'");
}

IN_PROC_BROWSER_TEST_F(LoginUITest, PRE_InterruptedAutoStartEnrollment) {
  StartupUtils::MarkOobeCompleted();
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kDeviceEnrollmentAutoStart, true);
  prefs->SetBoolean(prefs::kDeviceEnrollmentCanExit, false);
}

// Tests that the default first screen is the welcome screen after OOBE
// when auto enrollment is enabled and device is not yet enrolled.
IN_PROC_BROWSER_TEST_F(LoginUITest, InterruptedAutoStartEnrollment) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(LoginUITest, OobeNoExceptions) {
  test::OobeJS().ExpectTrue("cr.ErrorStore.getInstance().length == 0");
}

IN_PROC_BROWSER_TEST_F(LoginUITest, PRE_LoginNoExceptions) {
  RegisterUser(test_users_[0]);
  RegisterUser(test_users_[1]);
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(LoginUITest, LoginNoExceptions) {
  OobeScreenWaiter(OobeScreen::SCREEN_ACCOUNT_PICKER).Wait();
  test::OobeJS().ExpectTrue("cr.ErrorStore.getInstance().length == 0");
}

IN_PROC_BROWSER_TEST_F(LoginUITest, OobeCatchException) {
  test::OobeJS().ExpectTrue("cr.ErrorStore.getInstance().length == 0");
  test::OobeJS().ExecuteAsync("aelrt('misprint')");
  test::OobeJS().ExpectTrue("cr.ErrorStore.getInstance().length == 1");
  test::OobeJS().ExecuteAsync("consle.error('Some error')");
  test::OobeJS().ExpectTrue("cr.ErrorStore.getInstance().length == 2");
}

class LoginViewUITest : public LoginManagerTest {
 public:
  LoginViewUITest()
      : LoginManagerTest(false, false /* should_initialize_webui */) {
    set_force_webui_login(false);
    for (size_t i = 0; i < 10; ++i) {
      const std::string email = "a" + base::NumberToString(i) + "@gmail.com";
      const std::string gaia_id = base::NumberToString(i) + "111111111";
      test_users_.push_back(AccountId::FromUserEmailGaiaId(email, gaia_id));
    }
  }
  ~LoginViewUITest() override = default;

 protected:
  void AddUsers() {
    for (const auto& user : test_users_)
      RegisterUser(user);
  }
  std::vector<AccountId> test_users_;
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

IN_PROC_BROWSER_TEST_F(LoginViewUITest, PRE_UserRemoval) {
  AddUsers();
}

IN_PROC_BROWSER_TEST_F(LoginViewUITest, UserRemoval) {
  const int users_count = test_users_.size();
  EXPECT_EQ(users_count, ash::LoginScreenTestApi::GetUsersCount());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsOobeDialogVisible());

  // Remove the first user.
  EXPECT_TRUE(ash::LoginScreenTestApi::RemoveUser(test_users_[0]));
  EXPECT_EQ(users_count - 1, ash::LoginScreenTestApi::GetUsersCount());

  // Can not remove twice.
  EXPECT_FALSE(ash::LoginScreenTestApi::RemoveUser(test_users_[0]));
  EXPECT_EQ(users_count - 1, ash::LoginScreenTestApi::GetUsersCount());

  for (int i = 1; i < users_count; ++i) {
    EXPECT_TRUE(ash::LoginScreenTestApi::RemoveUser(test_users_[i]));
    EXPECT_EQ(users_count - i - 1, ash::LoginScreenTestApi::GetUsersCount());
  }

  // Gaia dialog should be shown again as there are no users anymore.
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
}

IN_PROC_BROWSER_TEST_F(LoginViewUITest, PRE_UserReverseRemoval) {
  AddUsers();
}

IN_PROC_BROWSER_TEST_F(LoginViewUITest, UserReverseRemoval) {
  const int users_count = test_users_.size();
  EXPECT_EQ(users_count, ash::LoginScreenTestApi::GetUsersCount());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsOobeDialogVisible());

  for (int i = users_count - 1; i >= 0; --i) {
    EXPECT_TRUE(ash::LoginScreenTestApi::RemoveUser(test_users_[i]));
    EXPECT_EQ(i, ash::LoginScreenTestApi::GetUsersCount());
  }

  // Gaia dialog should be shown again as there are no users anymore.
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
}

}  // namespace chromeos
