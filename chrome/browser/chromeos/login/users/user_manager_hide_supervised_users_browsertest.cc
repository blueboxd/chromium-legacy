// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_impl.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_base.h"
#include "content/public/test/browser_test.h"

namespace chromeos {

namespace {

struct {
  const char* email;
  const char* gaia_id;
} const kTestUsers[] = {
    {"test-user1@gmail.com", "1111111111"},
    {"test-user2@gmail.com", "2222222222"},
    // Test Supervised User.
    // The domain is defined in user_manager::kSupervisedUserDomain.
    // That const isn't directly referenced here to keep this code readable by
    // avoiding std::string concatenations.
    {"test-superviseduser@locally-managed.localhost", "3333333333"},
};

}  // namespace

class UserManagerHideSupervisedUsersBrowserTest : public LoginManagerTest {
 public:
  UserManagerHideSupervisedUsersBrowserTest() = default;

  ~UserManagerHideSupervisedUsersBrowserTest() override = default;

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();
    InitializeTestUsers();
  }

  void TearDownOnMainThread() override {
    LoginManagerTest::TearDownOnMainThread();
    scoped_user_manager_.reset();
  }

 protected:
  std::vector<AccountId> test_users_;
  ScopedTestingCrosSettings scoped_testing_cros_settings_;

 private:
  void InitializeTestUsers() {
    for (size_t i = 0; i < base::size(kTestUsers); ++i) {
      test_users_.emplace_back(AccountId::FromUserEmailGaiaId(
          kTestUsers[i].email, kTestUsers[i].gaia_id));
      RegisterUser(test_users_[i]);
    }

    // Setup a scoped real ChromeUserManager, since this is an end-to-end
    // test.  This will read the users registered to the local state just
    // before this in RegisterUser.
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        chromeos::ChromeUserManagerImpl::CreateChromeUserManager());
  }

  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;

  DISALLOW_COPY_AND_ASSIGN(UserManagerHideSupervisedUsersBrowserTest);
};

// The following subclasses set the  HideSupervisedUsers to various values for
// testing.  This could have been achieved through the use of a boolean test
// param, but that implementation would make the role of the flag itself less
// obvious, even though it would be less code.  Instead, there are 2 separate
// test subclasses which explictly state the flag's expected configuration.

// The flag is "HideSupervisedUsers", so this test class
// *enables* it, therefore *disabling* SupervisedUsers.
class UserManagerHideSupervisedUsersEnabledBrowserTest
    : public UserManagerHideSupervisedUsersBrowserTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    // This flag must be set here in order to be correctly
    // set during the test.
    scoped_feature_list_.InitAndEnableFeature(
        user_manager::kHideSupervisedUsers);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The flag is "HideSupervisedUsers", so this test class
// *disables* it, therefore *enabling* SupervisedUsers.
class UserManagerHideSupervisedUsersDisabledBrowserTest
    : public UserManagerHideSupervisedUsersBrowserTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    // This flag must be set here in order to be correctly
    // set during the test.
    scoped_feature_list_.InitAndDisableFeature(
        user_manager::kHideSupervisedUsers);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(UserManagerHideSupervisedUsersEnabledBrowserTest,
                       SupervisedUserNotReturned) {
  // When Supervised users are hidden, only 2 test users should be returned.
  EXPECT_EQ(2U, user_manager::UserManager::Get()->GetUsers().size());
}

IN_PROC_BROWSER_TEST_F(UserManagerHideSupervisedUsersDisabledBrowserTest,
                       SupervisedUserReturned) {
  // When Supervised users are not hidden, all 3 test users should be returned.
  EXPECT_EQ(3U, user_manager::UserManager::Get()->GetUsers().size());
}

}  // namespace chromeos
