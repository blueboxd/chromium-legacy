// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_features.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/fake_crostini_features.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crostini {

TEST(CrostiniFeaturesTest, TestFakeReplaces) {
  CrostiniFeatures* original = CrostiniFeatures::Get();
  {
    FakeCrostiniFeatures crostini_features;
    EXPECT_NE(original, CrostiniFeatures::Get());
    EXPECT_EQ(&crostini_features, CrostiniFeatures::Get());
  }
  EXPECT_EQ(original, CrostiniFeatures::Get());
}

TEST(CrostiniFeaturesTest, TestExportImportUIAllowed) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  FakeCrostiniFeatures crostini_features;

  // Set up for success.
  crostini_features.set_ui_allowed(true);
  profile.GetPrefs()->SetBoolean(
      crostini::prefs::kUserCrostiniExportImportUIAllowedByPolicy, true);

  // Success.
  EXPECT_TRUE(crostini_features.IsExportImportUIAllowed(&profile));

  // Crostini UI not allowed.
  crostini_features.set_ui_allowed(false);
  EXPECT_FALSE(crostini_features.IsExportImportUIAllowed(&profile));
  crostini_features.set_ui_allowed(true);

  // Pref off.
  profile.GetPrefs()->SetBoolean(
      crostini::prefs::kUserCrostiniExportImportUIAllowedByPolicy, false);
  EXPECT_FALSE(crostini_features.IsExportImportUIAllowed(&profile));
}

TEST(CrostiniFeaturesTest, TestRootAccessAllowed) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  FakeCrostiniFeatures crostini_features;
  base::test::ScopedFeatureList scoped_feature_list;

  // Set up for success.
  crostini_features.set_ui_allowed(true);
  scoped_feature_list.InitWithFeatures(
      {features::kCrostiniAdvancedAccessControls}, {});
  profile.GetPrefs()->SetBoolean(
      crostini::prefs::kUserCrostiniRootAccessAllowedByPolicy, true);

  // Success.
  EXPECT_TRUE(crostini_features.IsRootAccessAllowed(&profile));

  // Pref off.
  profile.GetPrefs()->SetBoolean(
      crostini::prefs::kUserCrostiniRootAccessAllowedByPolicy, false);
  EXPECT_FALSE(crostini_features.IsRootAccessAllowed(&profile));

  // Feature disabled.
  {
    base::test::ScopedFeatureList feature_list_disabled;
    feature_list_disabled.InitWithFeatures(
        {}, {features::kCrostiniAdvancedAccessControls});
    EXPECT_TRUE(crostini_features.IsRootAccessAllowed(&profile));
  }
}

TEST(CrostiniFeaturesTest, TestCanChangeAdbSideloadingChildUser) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  FakeCrostiniFeatures crostini_features;

  auto scoped_user_manager = std::make_unique<user_manager::ScopedUserManager>(
      std::make_unique<chromeos::FakeChromeUserManager>());
  auto* user_manager = static_cast<chromeos::FakeChromeUserManager*>(
      user_manager::UserManager::Get());
  AccountId account_id = AccountId::FromUserEmail(profile.GetProfileUserName());
  auto* const user = user_manager->AddChildUser(account_id);
  user_manager->UserLoggedIn(account_id, user->username_hash(),
                             /*browser_restart=*/false,
                             /*is_child=*/true);

  EXPECT_FALSE(crostini_features.CanChangeAdbSideloading(&profile));
}

TEST(CrostiniFeaturesTest,
     TestCanChangeAdbSideloadingManagedDisabledFeatureFlag) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  FakeCrostiniFeatures crostini_features;

  // Disable feature flag
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {}, {chromeos::features::kArcManagedAdbSideloadingSupport});

  EXPECT_FALSE(crostini_features.CanChangeAdbSideloading(&profile));
}

TEST(CrostiniFeaturesTest, TestCanChangeAdbSideloadingManagedDeviceAndUser) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  FakeCrostiniFeatures crostini_features;

  // Enable feature flag
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {chromeos::features::kArcManagedAdbSideloadingSupport}, {});

  // Set device to enterprise-managed
  profile.ScopedCrosSettingsTestHelper()->InstallAttributes()->SetCloudManaged(
      "domain.com", "device_id");

  // Set profile to enterprise-managed
  profile.GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);

  // TODO(janagrill): Once the device and user policies are implemented,
  // adjust the test accordingly
  EXPECT_TRUE(crostini_features.CanChangeAdbSideloading(&profile));
}

TEST(CrostiniFeaturesTest, TestCanChangeAdbSideloadingOwnerProfile) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  FakeCrostiniFeatures crostini_features;

  // Set device to not enterprise-managed
  profile.ScopedCrosSettingsTestHelper()
      ->InstallAttributes()
      ->SetConsumerOwned();

  // Set profile to not enterprise-managed
  profile.GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);

  // Set profile to owner
  auto scoped_user_manager = std::make_unique<user_manager::ScopedUserManager>(
      std::make_unique<chromeos::FakeChromeUserManager>());
  auto* user_manager = static_cast<chromeos::FakeChromeUserManager*>(
      user_manager::UserManager::Get());
  AccountId account_id = AccountId::FromUserEmail(profile.GetProfileUserName());
  user_manager->AddUser(account_id);
  user_manager->LoginUser(account_id);
  user_manager->SetOwnerId(account_id);

  EXPECT_TRUE(crostini_features.CanChangeAdbSideloading(&profile));
}

TEST(CrostiniFeaturesTest, TestCanChangeAdbSideloadingOwnerProfileManagedUser) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  FakeCrostiniFeatures crostini_features;

  // Enable feature flag
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {chromeos::features::kArcManagedAdbSideloadingSupport}, {});

  // Set device to not enterprise-managed
  profile.ScopedCrosSettingsTestHelper()
      ->InstallAttributes()
      ->SetConsumerOwned();

  // Set profile to enterprise-managed
  profile.GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);

  // Set profile to owner
  auto scoped_user_manager = std::make_unique<user_manager::ScopedUserManager>(
      std::make_unique<chromeos::FakeChromeUserManager>());
  auto* user_manager = static_cast<chromeos::FakeChromeUserManager*>(
      user_manager::UserManager::Get());
  AccountId account_id = AccountId::FromUserEmail(profile.GetProfileUserName());
  user_manager->AddUser(account_id);
  user_manager->LoginUser(account_id);
  user_manager->SetOwnerId(account_id);

  // TODO(janagrill): Once the user policy is implemented, adjust the test
  EXPECT_TRUE(crostini_features.CanChangeAdbSideloading(&profile));
}

}  // namespace crostini
