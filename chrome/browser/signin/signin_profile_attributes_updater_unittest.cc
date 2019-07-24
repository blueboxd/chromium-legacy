// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_profile_attributes_updater.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kEmail[] = "example@email.com";

}  // namespace

class SigninProfileAttributesUpdaterTest : public testing::Test {
 public:
  SigninProfileAttributesUpdaterTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        signin_error_controller_(
            SigninErrorController::AccountMode::PRIMARY_ACCOUNT,
            identity_test_env_.identity_manager()) {}

  // Recreates |signin_profile_attributes_updater_|. Useful for tests that want
  // to set up the updater with specific preconditions.
  void RecreateSigninProfileAttributesUpdater() {
    signin_profile_attributes_updater_ =
        std::make_unique<SigninProfileAttributesUpdater>(
            identity_test_env_.identity_manager(), &signin_error_controller_,
            profile_manager_.profile_attributes_storage(), profile_path_);
  }

  void SetUp() override {
    testing::Test::SetUp();

    ASSERT_TRUE(profile_manager_.SetUp());
    std::string name = "profile_name";
    profile_path_ = profile_manager_
                        .CreateTestingProfile(
                            name, /*prefs=*/nullptr, base::UTF8ToUTF16(name), 0,
                            std::string(), TestingProfile::TestingFactories())
                        ->GetPath();

    RecreateSigninProfileAttributesUpdater();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfileManager profile_manager_;
  base::FilePath profile_path_;
  signin::IdentityTestEnvironment identity_test_env_;
  SigninErrorController signin_error_controller_;
  std::unique_ptr<SigninProfileAttributesUpdater>
      signin_profile_attributes_updater_;
};

#if !defined(OS_CHROMEOS)
// Tests that the browser state info is updated on signin and signout.
// ChromeOS does not support signout.
TEST_F(SigninProfileAttributesUpdaterTest, SigninSignout) {
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(profile_manager_.profile_attributes_storage()
                  ->GetProfileAttributesWithPath(profile_path_, &entry));
  ASSERT_FALSE(entry->IsAuthenticated());
  EXPECT_FALSE(entry->IsSigninRequired());

  // Signin.
  identity_test_env_.MakePrimaryAccountAvailable(kEmail);
  EXPECT_TRUE(entry->IsAuthenticated());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kEmail), entry->GetGAIAId());
  EXPECT_EQ(kEmail, base::UTF16ToUTF8(entry->GetUserName()));

  // Signout.
  identity_test_env_.ClearPrimaryAccount();
  EXPECT_FALSE(entry->IsAuthenticated());
  EXPECT_FALSE(entry->IsSigninRequired());
}
#endif  // !defined(OS_CHROMEOS)

// Tests that the browser state info is updated on auth error change.
TEST_F(SigninProfileAttributesUpdaterTest, AuthError) {
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(profile_manager_.profile_attributes_storage()
                  ->GetProfileAttributesWithPath(profile_path_, &entry));

  std::string account_id =
      identity_test_env_.MakePrimaryAccountAvailable(kEmail).account_id;

#if defined(OS_CHROMEOS)
  // ChromeOS only observes signin state at initial creation of the updater, so
  // recreate the updater after having set the primary account.
  RecreateSigninProfileAttributesUpdater();
#endif

  EXPECT_TRUE(entry->IsAuthenticated());
  EXPECT_FALSE(entry->IsAuthError());

  // Set auth error.
  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_TRUE(entry->IsAuthError());

  // Remove auth error.
  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account_id, GoogleServiceAuthError::AuthErrorNone());
  EXPECT_FALSE(entry->IsAuthError());
}

#if !defined(OS_CHROMEOS)
class SigninProfileAttributesUpdaterWithForceSigninTest
    : public SigninProfileAttributesUpdaterTest {
  void SetUp() override {
    signin_util::SetForceSigninForTesting(true);
    SigninProfileAttributesUpdaterTest::SetUp();
  }

  void TearDown() override {
    SigninProfileAttributesUpdaterTest::TearDown();
    signin_util::ResetForceSigninForTesting();
  }
};

TEST_F(SigninProfileAttributesUpdaterWithForceSigninTest, IsSigninRequired) {
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(profile_manager_.profile_attributes_storage()
                  ->GetProfileAttributesWithPath(profile_path_, &entry));
  EXPECT_FALSE(entry->IsAuthenticated());
  EXPECT_TRUE(entry->IsSigninRequired());

  AccountInfo account_info =
      identity_test_env_.MakePrimaryAccountAvailable(kEmail);

  EXPECT_TRUE(entry->IsAuthenticated());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kEmail), entry->GetGAIAId());
  EXPECT_EQ(kEmail, base::UTF16ToUTF8(entry->GetUserName()));

  identity_test_env_.ClearPrimaryAccount();
  EXPECT_FALSE(entry->IsAuthenticated());
  EXPECT_TRUE(entry->IsSigninRequired());
}
#endif
