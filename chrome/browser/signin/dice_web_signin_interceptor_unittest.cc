// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_web_signin_interceptor.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// If the account info is valid, does nothing. Otherwise fills the extended
// fields with default values.
void MakeValidAccountInfo(AccountInfo* info) {
  if (info->IsValid())
    return;
  info->full_name = "fullname";
  info->given_name = "givenname";
  info->hosted_domain = kNoHostedDomainFound;
  info->locale = "en";
  info->picture_url = "https://example.com";
  info->is_child_account = false;
  DCHECK(info->IsValid());
}

}  // namespace

class DiceWebSigninInterceptorTest : public testing::Test {
 public:
  DiceWebSigninInterceptorTest() = default;
  ~DiceWebSigninInterceptorTest() override = default;

  DiceWebSigninInterceptor* interceptor() {
    return dice_web_signin_interceptor_.get();
  }

  Profile* profile() { return profile_; }

  ProfileAttributesStorage* profile_attributes_storage() {
    return profile_manager_->profile_attributes_storage();
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

  Profile* CreateTestingProfile(const std::string& name) {
    return profile_manager_->CreateTestingProfile(name);
  }

 private:
  // testing::Test:
  void SetUp() override {
    // Create a testing profile registered in the profile manager.
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    TestingProfile::TestingFactories factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    factories.push_back(
        {ChromeSigninClientFactory::GetInstance(),
         base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                             &test_url_loader_factory_)});
    profile_ = profile_manager_->CreateTestingProfile(
        chrome::kInitialProfile,
        std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
        base::UTF8ToUTF16(""), 0, std::string(), std::move(factories));
    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);
    identity_test_env_profile_adaptor_->identity_test_env()
        ->SetTestURLLoaderFactory(&test_url_loader_factory_);

    dice_web_signin_interceptor_ =
        std::make_unique<DiceWebSigninInterceptor>(profile_);
  }

  void TearDown() override {
    dice_web_signin_interceptor_->Shutdown();
    profile_ = nullptr;
    identity_test_env_profile_adaptor_.reset();
    profile_manager_->DeleteTestingProfile(chrome::kInitialProfile);
  }

  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_ = nullptr;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  std::unique_ptr<DiceWebSigninInterceptor> dice_web_signin_interceptor_;
};

TEST_F(DiceWebSigninInterceptorTest, IsAccountInAnotherProfile) {
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  EXPECT_FALSE(interceptor()->IsAccountInAnotherProfile(
      account_info, profile_attributes_storage()));

  // Add another profile with no account.
  CreateTestingProfile("Profile 1");
  EXPECT_FALSE(interceptor()->IsAccountInAnotherProfile(
      account_info, profile_attributes_storage()));

  // Add another profile with a different account.
  Profile* profile_2 = CreateTestingProfile("Profile 2");
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(profile_attributes_storage()->GetProfileAttributesWithPath(
      profile_2->GetPath(), &entry));
  std::string kOtherGaiaID = "SomeOtherGaiaID";
  ASSERT_NE(kOtherGaiaID, account_info.gaia);
  entry->SetAuthInfo(kOtherGaiaID, base::UTF8ToUTF16("Bob"),
                     /*is_consented_primary_account=*/true);
  EXPECT_FALSE(interceptor()->IsAccountInAnotherProfile(
      account_info, profile_attributes_storage()));

  // Change the account to match.
  entry->SetAuthInfo(account_info.gaia, base::UTF8ToUTF16("Bob"),
                     /*is_consented_primary_account=*/false);
  EXPECT_TRUE(interceptor()->IsAccountInAnotherProfile(
      account_info, profile_attributes_storage()));
}

TEST_F(DiceWebSigninInterceptorTest, NoBubbleWithSingleAccount) {
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  MakeValidAccountInfo(&account_info);
  account_info.hosted_domain = "example.com";
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Without UPA.
  EXPECT_FALSE(interceptor()->ShouldShowEnterpriseBubble(account_info));
  EXPECT_FALSE(interceptor()->ShouldShowMultiUserBubble(account_info));

  // With UPA.
  identity_test_env()->SetUnconsentedPrimaryAccount("bob@example.com");
  EXPECT_FALSE(interceptor()->ShouldShowEnterpriseBubble(account_info));
}

TEST_F(DiceWebSigninInterceptorTest, ShouldShowEnterpriseBubble) {
  // Setup 3 accounts in the profile:
  // - primary account
  // - other enterprise account that is not primary (should be ignored)
  // - intercepted account.
  AccountInfo primary_account_info =
      identity_test_env()->MakeUnconsentedPrimaryAccountAvailable(
          "alice@example.com");
  AccountInfo other_account_info =
      identity_test_env()->MakeAccountAvailable("dummy@example.com");
  MakeValidAccountInfo(&other_account_info);
  other_account_info.hosted_domain = "example.com";
  identity_test_env()->UpdateAccountInfoForAccount(other_account_info);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  ASSERT_EQ(identity_test_env()->identity_manager()->GetPrimaryAccountId(
                signin::ConsentLevel::kNotRequired),
            primary_account_info.account_id);

  // The primary account does not have full account info (empty domain).
  ASSERT_TRUE(identity_test_env()
                  ->identity_manager()
                  ->FindExtendedAccountInfoForAccountWithRefreshToken(
                      primary_account_info)
                  ->hosted_domain.empty());
  EXPECT_FALSE(interceptor()->ShouldShowEnterpriseBubble(account_info));
  account_info.hosted_domain = "example.com";
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  EXPECT_TRUE(interceptor()->ShouldShowEnterpriseBubble(account_info));

  // The primary account has full info.
  MakeValidAccountInfo(&primary_account_info);
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_info);
  // The intercepted account is enterprise.
  EXPECT_TRUE(interceptor()->ShouldShowEnterpriseBubble(account_info));
  // Two consummer accounts.
  account_info.hosted_domain = kNoHostedDomainFound;
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  EXPECT_FALSE(interceptor()->ShouldShowEnterpriseBubble(account_info));
  // The primary account is enterprise.
  primary_account_info.hosted_domain = "example.com";
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_info);
  EXPECT_TRUE(interceptor()->ShouldShowEnterpriseBubble(account_info));
}

TEST_F(DiceWebSigninInterceptorTest, ShouldShowEnterpriseBubbleWithoutUPA) {
  AccountInfo account_info_1 =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  MakeValidAccountInfo(&account_info_1);
  account_info_1.hosted_domain = "example.com";
  identity_test_env()->UpdateAccountInfoForAccount(account_info_1);
  AccountInfo account_info_2 =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&account_info_2);
  account_info_2.hosted_domain = "example.com";
  identity_test_env()->UpdateAccountInfoForAccount(account_info_2);

  // Primary account is not set.
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kNotRequired));
  EXPECT_FALSE(interceptor()->ShouldShowEnterpriseBubble(account_info_1));
}

TEST_F(DiceWebSigninInterceptorTest, ShouldShowMultiUserBubble) {
  // Setup two accounts in the profile.
  AccountInfo account_info_1 =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  MakeValidAccountInfo(&account_info_1);
  account_info_1.given_name = "Bob";
  identity_test_env()->UpdateAccountInfoForAccount(account_info_1);
  AccountInfo account_info_2 =
      identity_test_env()->MakeAccountAvailable("alice@example.com");

  // The other account does not have full account info (empty name).
  ASSERT_TRUE(account_info_2.given_name.empty());
  EXPECT_TRUE(interceptor()->ShouldShowMultiUserBubble(account_info_1));

  // Accounts with different names.
  account_info_1.given_name = "Bob";
  identity_test_env()->UpdateAccountInfoForAccount(account_info_1);
  MakeValidAccountInfo(&account_info_2);
  account_info_2.given_name = "Alice";
  identity_test_env()->UpdateAccountInfoForAccount(account_info_2);
  EXPECT_TRUE(interceptor()->ShouldShowMultiUserBubble(account_info_1));

  // Accounts with same names.
  account_info_1.given_name = "Alice";
  identity_test_env()->UpdateAccountInfoForAccount(account_info_1);
  EXPECT_FALSE(interceptor()->ShouldShowMultiUserBubble(account_info_1));

  // Comparison is case insensitive.
  account_info_1.given_name = "alice";
  identity_test_env()->UpdateAccountInfoForAccount(account_info_1);
  EXPECT_FALSE(interceptor()->ShouldShowMultiUserBubble(account_info_1));
}
