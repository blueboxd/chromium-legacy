// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager_base.h"
#include "content/public/test/browser_test.h"

namespace chromeos {

class LoginUIHideSupervisedUsersTest : public LoginManagerTest {
 public:
  LoginUIHideSupervisedUsersTest() : LoginManagerTest() {
    login_mixin_.AppendRegularUsers(2);
    login_mixin_.AppendLegacySupervisedUsers(1);
  }

 protected:
  LoginManagerMixin login_mixin_{&mixin_host_};
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

// Verifies that Supervised Users are *not* displayed on the login screen when
// the HideSupervisedUsers feature flag *is* enabled.
IN_PROC_BROWSER_TEST_F(LoginUIHideSupervisedUsersEnabledTest,
                       SupervisedUserHidden) {
  const auto& test_users = login_mixin_.users();
  // Only the regular users should be displayed on the login screen.
  EXPECT_EQ(2, ash::LoginScreenTestApi::GetUsersCount());
  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(test_users[0].account_id));
  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(test_users[1].account_id));
}

// Verifies that Supervised Users *are* displayed on the login screen when the
// HideSupervisedUsers feature flag is *not* enabled.
IN_PROC_BROWSER_TEST_F(LoginUIHideSupervisedUsersDisabledTest,
                       SupervisedUserDisplayed) {
  const auto& test_users = login_mixin_.users();
  EXPECT_EQ(3, ash::LoginScreenTestApi::GetUsersCount());
  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(test_users[0].account_id));
  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(test_users[1].account_id));
  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(test_users[2].account_id));
}

}  // namespace chromeos
