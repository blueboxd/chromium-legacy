// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/gaia_info_update_service.h"

#include <stddef.h>

#include <string>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_downloader.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_info_cache_unittest.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

using ::testing::Return;

namespace {

#if !defined(OS_CHROMEOS)
AccountInfo GetValidAccountInfo(std::string email,
                                CoreAccountId account_id,
                                std::string given_name,
                                std::string full_name) {
  AccountInfo account_info;
  account_info.email = email;
  account_info.gaia = account_id.ToString();
  account_info.account_id = account_id;
  account_info.given_name = given_name;
  account_info.full_name = full_name;
  account_info.hosted_domain = kNoHostedDomainFound;
  account_info.locale = email;
  account_info.picture_url = "example.com";
  return account_info;
}
#endif  // !defined(OS_CHROMEOS)

class GAIAInfoUpdateServiceTest : public testing::Test {
 protected:
  GAIAInfoUpdateServiceTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~GAIAInfoUpdateServiceTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    RecreateGAIAInfoUpdateService();
  }

  void RecreateGAIAInfoUpdateService() {
    if (service_)
      service_->Shutdown();

    service_.reset(new GAIAInfoUpdateService(
        identity_test_env_.identity_manager(),
        testing_profile_manager_.profile_attributes_storage(),
        profile()->GetPath(), profile()->GetPrefs()));
  }

  void TearDown() override {
    if (service_) {
      service_->Shutdown();
      service_.reset();
    }
  }

  TestingProfile* profile() {
    if (!profile_)
      CreateProfile("Person 1");
    return profile_;
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  ProfileAttributesStorage* storage() {
    return testing_profile_manager_.profile_attributes_storage();
  }

  GAIAInfoUpdateService* service() { return service_.get(); }

  void CreateProfile(const std::string& name) {
    profile_ = testing_profile_manager_.CreateTestingProfile(
        name, std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
        base::UTF8ToUTF16(name), 0, std::string(),
        TestingProfile::TestingFactories());
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
  TestingProfile* profile_ = nullptr;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<GAIAInfoUpdateService> service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GAIAInfoUpdateServiceTest);
};
}  // namespace

TEST_F(GAIAInfoUpdateServiceTest, ShouldUseGAIAProfileInfo) {
#if defined(OS_CHROMEOS)
  // This feature should never be enabled on ChromeOS.
  EXPECT_FALSE(GAIAInfoUpdateService::ShouldUseGAIAProfileInfo(profile()));
#endif
}

#if !defined(OS_CHROMEOS)

TEST_F(GAIAInfoUpdateServiceTest, SyncOnSyncOff) {
  AccountInfo info =
      identity_test_env()->MakeAccountAvailable("pat@example.com");
  base::RunLoop().RunUntilIdle();
  identity_test_env()->SetPrimaryAccount(info.email);
  info = GetValidAccountInfo(info.email, info.account_id, "Pat", "Pat Foo");
  signin::UpdateAccountInfoForAccount(identity_test_env()->identity_manager(),
                                      info);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, storage()->GetNumberOfProfiles());
  ProfileAttributesEntry* entry = storage()->GetAllProfilesAttributes().front();
  EXPECT_EQ(entry->GetGAIAGivenName(), base::UTF8ToUTF16("Pat"));
  EXPECT_EQ(entry->GetGAIAName(), base::UTF8ToUTF16("Pat Foo"));
  EXPECT_EQ(
      profile()->GetPrefs()->GetString(prefs::kGoogleServicesHostedDomain),
      info.hosted_domain);

  gfx::Image gaia_picture = gfx::test::CreateImage(256, 256);
  signin::SimulateAccountImageFetch(identity_test_env()->identity_manager(),
                                    info.account_id, gaia_picture);
  // Set a fake picture URL.
  EXPECT_TRUE(gfx::test::AreImagesEqual(gaia_picture, entry->GetAvatarIcon()));
  // Log out.
  identity_test_env()->ClearPrimaryAccount();
  // Verify that the GAIA name and picture, and picture URL are unset.
  EXPECT_TRUE(entry->GetGAIAGivenName().empty());
  EXPECT_TRUE(entry->GetGAIAName().empty());
  EXPECT_EQ(nullptr, entry->GetGAIAPicture());
  EXPECT_TRUE(profile()
                  ->GetPrefs()
                  ->GetString(prefs::kGoogleServicesHostedDomain)
                  .empty());
}

#if !defined(ANDROID)
TEST_F(GAIAInfoUpdateServiceTest, LogInLogOut) {
  std::string email = "pat@example.com";
  AccountInfo info = identity_test_env()->MakeAccountAvailableWithCookies(
      email, signin::GetTestGaiaIdForEmail(email));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      identity_test_env()->identity_manager()->HasUnconsentedPrimaryAccount());
  EXPECT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount());
  info = GetValidAccountInfo(info.email, info.account_id, "Pat", "Pat Foo");
  signin::UpdateAccountInfoForAccount(identity_test_env()->identity_manager(),
                                      info);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, storage()->GetNumberOfProfiles());
  ProfileAttributesEntry* entry = storage()->GetAllProfilesAttributes().front();
  EXPECT_EQ(entry->GetGAIAGivenName(), base::UTF8ToUTF16("Pat"));
  EXPECT_EQ(entry->GetGAIAName(), base::UTF8ToUTF16("Pat Foo"));
  EXPECT_EQ(
      profile()->GetPrefs()->GetString(prefs::kGoogleServicesHostedDomain),
      info.hosted_domain);

  gfx::Image gaia_picture = gfx::test::CreateImage(256, 256);
  signin::SimulateAccountImageFetch(identity_test_env()->identity_manager(),
                                    info.account_id, gaia_picture);
  // Set a fake picture URL.
  EXPECT_TRUE(gfx::test::AreImagesEqual(gaia_picture, entry->GetAvatarIcon()));
  // Log out.
  identity_test_env()->SetCookieAccounts({});
  // Verify that the GAIA name and picture, and picture URL are unset.
  EXPECT_TRUE(entry->GetGAIAGivenName().empty());
  EXPECT_TRUE(entry->GetGAIAName().empty());
  EXPECT_EQ(nullptr, entry->GetGAIAPicture());
  EXPECT_TRUE(profile()
                  ->GetPrefs()
                  ->GetString(prefs::kGoogleServicesHostedDomain)
                  .empty());
}
#endif  // !defined(ANDROID)

TEST_F(GAIAInfoUpdateServiceTest, ClearGaiaInfoOnStartup) {
  // Simulate a state where the profile entry has GAIA related information
  // when there is not primary account set.
  ASSERT_FALSE(
      identity_test_env()->identity_manager()->HasUnconsentedPrimaryAccount());
  ASSERT_EQ(1u, storage()->GetNumberOfProfiles());
  ProfileAttributesEntry* entry = storage()->GetAllProfilesAttributes().front();
  entry->SetGAIAName(base::UTF8ToUTF16("foo"));
  entry->SetGAIAGivenName(base::UTF8ToUTF16("Pat Foo"));
  gfx::Image gaia_picture = gfx::test::CreateImage(256, 256);
  entry->SetGAIAPicture(gaia_picture);

  // Verify that creating the GAIAInfoUpdateService resets the GAIA related
  // profile attributes if the profile no longer has a primary account and that
  // the profile info cache observer wass notified about profile name and
  // avatar changes.
  RecreateGAIAInfoUpdateService();

  EXPECT_TRUE(entry->GetGAIAName().empty());
  EXPECT_TRUE(entry->GetGAIAGivenName().empty());
  EXPECT_FALSE(entry->GetGAIAPicture());
}

#endif  // !defined(OS_CHROMEOS)
