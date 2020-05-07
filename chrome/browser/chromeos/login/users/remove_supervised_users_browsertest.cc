// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/common/chrome_features.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_base.h"
#include "content/public/test/browser_test.h"

namespace chromeos {

class RemoveSupervisedUsersBrowserTest : public LoginManagerTest {
 public:
  RemoveSupervisedUsersBrowserTest() : LoginManagerTest() {
    login_mixin_.AppendRegularUsers(2);
    login_mixin_.AppendLegacySupervisedUsers(1);
  }

  ~RemoveSupervisedUsersBrowserTest() override = default;

 protected:
  LoginManagerMixin login_mixin_{&mixin_host_};

  DISALLOW_COPY_AND_ASSIGN(RemoveSupervisedUsersBrowserTest);
};

class RemoveSupervisedUsersBrowserEnabledTest
    : public RemoveSupervisedUsersBrowserTest {
 public:
  RemoveSupervisedUsersBrowserEnabledTest()
      : RemoveSupervisedUsersBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kRemoveSupervisedUsersOnStartup},
        {user_manager::kHideSupervisedUsers});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class RemoveSupervisedUsersBrowserDisabledTest
    : public RemoveSupervisedUsersBrowserTest {
 public:
  RemoveSupervisedUsersBrowserDisabledTest()
      : RemoveSupervisedUsersBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {}, {user_manager::kHideSupervisedUsers,
             features::kRemoveSupervisedUsersOnStartup});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(RemoveSupervisedUsersBrowserEnabledTest,
                       SupervisedUserRemoved) {
  // When Supervised users are removed, only 2 test users should be returned.
  EXPECT_EQ(2U, user_manager::UserManager::Get()->GetUsers().size());

  // None of the non-supervised users should have been removed.
  for (user_manager::User* user :
       user_manager::UserManager::Get()->GetUsers()) {
    EXPECT_EQ(false, user->IsSupervised());
  }
}

IN_PROC_BROWSER_TEST_F(RemoveSupervisedUsersBrowserDisabledTest,
                       SupervisedUserNotRemoved) {
  // When Supervised users are not removed, all 3 test users should be returned.
  EXPECT_EQ(3U, user_manager::UserManager::Get()->GetUsers().size());
}

}  // namespace chromeos
