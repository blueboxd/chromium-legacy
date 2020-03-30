// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager_base.h"

namespace chromeos {

namespace {

struct {
  const char* email;
  const char* gaia_id;
} const kTestUsers[] = {
    {"test-user1@gmail.com", "1111111111"},
    {"test-user2@gmail.com", "2222222222"},
    // Test Supervised User.
    // The user's domain is defined in user_manager::kSupervisedUserDomain.
    // That const isn't directly referenced here to keep this code readable by
    // avoiding std::string concatenations.
    {"test-superviseduser@locally-managed.localhost", "3333333333"},
};

}  // namespace

class LoginUIHideSupervisedUsersTest : public LoginManagerTest {
 public:
  LoginUIHideSupervisedUsersTest()
      : LoginManagerTest(false, false /* should_initialize_webui */) {
    set_force_webui_login(false);
    for (size_t i = 0; i < base::size(kTestUsers); ++i) {
      test_users_.emplace_back(AccountId::FromUserEmailGaiaId(
          kTestUsers[i].email, kTestUsers[i].gaia_id));
    }
  }

 protected:
  std::vector<AccountId> test_users_;
};

// The flag is "HideSupervisedUsers", so this test class
// *enables* it, therefore *disabling* SupervisedUsers.
class LoginUIHideSupervisedUsersEnabledTest
    : public LoginUIHideSupervisedUsersTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitAndEnableFeature(
        user_manager::kHideSupervisedUsers);
    LoginUIHideSupervisedUsersTest::SetUpInProcessBrowserTestFixture();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The flag is "HideSupervisedUsers", so this test class
// *disables* it, therefore *enabling* SupervisedUsers.
class LoginUIHideSupervisedUsersDisabledTest
    : public LoginUIHideSupervisedUsersTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitAndDisableFeature(
        user_manager::kHideSupervisedUsers);
    LoginUIHideSupervisedUsersTest::SetUpInProcessBrowserTestFixture();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(LoginUIHideSupervisedUsersEnabledTest,
                       PRE_SupervisedUserHidden) {
  RegisterUser(test_users_[0]);
  RegisterUser(test_users_[1]);
  RegisterUser(test_users_[2]);  // The test Supervised User.
  StartupUtils::MarkOobeCompleted();
}

// Verifies that Supervised Users are *not* displayed on the login screen when
// the HideSupervisedUsers feature flag *is* enabled.
IN_PROC_BROWSER_TEST_F(LoginUIHideSupervisedUsersEnabledTest,
                       SupervisedUserHidden) {
  // Only the regular users should be displayed on the login screen.
  EXPECT_EQ(2, ash::LoginScreenTestApi::GetUsersCount());
  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(test_users_[0]));
  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(test_users_[1]));
}

IN_PROC_BROWSER_TEST_F(LoginUIHideSupervisedUsersDisabledTest,
                       PRE_SupervisedUserDisplayed) {
  RegisterUser(test_users_[0]);
  RegisterUser(test_users_[1]);
  RegisterUser(test_users_[2]);  // The test Supervised User.
  StartupUtils::MarkOobeCompleted();
}

// Verifies that Supervised Users *are* displayed on the login screen when the
// HideSupervisedUsers feature flag is *not* enabled.
IN_PROC_BROWSER_TEST_F(LoginUIHideSupervisedUsersDisabledTest,
                       SupervisedUserDisplayed) {
  EXPECT_EQ(3, ash::LoginScreenTestApi::GetUsersCount());
  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(test_users_[0]));
  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(test_users_[1]));
  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(test_users_[2]));
}

}  // namespace chromeos
