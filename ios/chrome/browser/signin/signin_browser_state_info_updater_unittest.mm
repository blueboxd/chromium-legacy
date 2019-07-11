// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/signin_browser_state_info_updater.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/browser_state_info_cache.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#include "ios/chrome/browser/signin/identity_test_environment_chrome_browser_state_adaptor.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

const char kEmail[] = "example@email.com";

}  // namespace

class SigninBrowserStateInfoUpdaterTest : public PlatformTest {
 public:
  SigninBrowserStateInfoUpdaterTest() {
    EXPECT_TRUE(temp_directory_.CreateUniqueTempDir());
    // Setup the browser state manager.
    scoped_browser_state_manager_ =
        std::make_unique<IOSChromeScopedTestingChromeBrowserStateManager>(
            std::make_unique<TestChromeBrowserStateManager>(
                temp_directory_.GetPath().DirName()));
    browser_state_info_ = GetApplicationContext()
                              ->GetChromeBrowserStateManager()
                              ->GetBrowserStateInfoCache();
    browser_state_info_->AddBrowserState(temp_directory_.GetPath(),
                                         /*gaia_id=*/std::string(),
                                         /*user_name=*/base::string16());
    // Create the browser state.
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.SetPath(temp_directory_.GetPath());
    chrome_browser_state_ = IdentityTestEnvironmentChromeBrowserStateAdaptor::
        CreateChromeBrowserStateForIdentityTestEnvironment(test_cbs_builder);

    identity_test_environment_chrome_browser_state_adaptor_ =
        std::make_unique<IdentityTestEnvironmentChromeBrowserStateAdaptor>(
            chrome_browser_state_.get());

    cache_index_ = browser_state_info_->GetIndexOfBrowserStateWithPath(
        chrome_browser_state_->GetStatePath());
  }

  identity::IdentityTestEnvironment* identity_test_env() {
    return identity_test_environment_chrome_browser_state_adaptor_
        ->identity_test_env();
  }

  web::TestWebThreadBundle thread_bundle_;
  base::ScopedTempDir temp_directory_;
  std::unique_ptr<IOSChromeScopedTestingChromeBrowserStateManager>
      scoped_browser_state_manager_;
  size_t cache_index_ = 0u;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<IdentityTestEnvironmentChromeBrowserStateAdaptor>
      identity_test_environment_chrome_browser_state_adaptor_;

  // Weak, owned by scoped_browser_state_manager_.
  BrowserStateInfoCache* browser_state_info_ = nullptr;
};

// Tests that the browser state info is updated on signin and signout.
TEST_F(SigninBrowserStateInfoUpdaterTest, SigninSignout) {
  ASSERT_FALSE(
      browser_state_info_->BrowserStateIsAuthenticatedAtIndex(cache_index_));

  // Signin.
  AccountInfo account_info =
      identity_test_env()->MakePrimaryAccountAvailable(kEmail);

  EXPECT_TRUE(
      browser_state_info_->BrowserStateIsAuthenticatedAtIndex(cache_index_));
  EXPECT_EQ(account_info.gaia,
            browser_state_info_->GetGAIAIdOfBrowserStateAtIndex(cache_index_));
  EXPECT_EQ(
      kEmail,
      base::UTF16ToUTF8(
          browser_state_info_->GetUserNameOfBrowserStateAtIndex(cache_index_)));

  // Signout.
  identity_test_env()->ClearPrimaryAccount();
  EXPECT_FALSE(
      browser_state_info_->BrowserStateIsAuthenticatedAtIndex(cache_index_));
}

// Tests that the browser state info is updated on auth error change.
TEST_F(SigninBrowserStateInfoUpdaterTest, AuthError) {
  ASSERT_FALSE(
      browser_state_info_->BrowserStateIsAuthenticatedAtIndex(cache_index_));

  // Signin.
  AccountInfo account_info =
      identity_test_env()->MakePrimaryAccountAvailable(kEmail);

  EXPECT_TRUE(
      browser_state_info_->BrowserStateIsAuthenticatedAtIndex(cache_index_));
  EXPECT_FALSE(
      browser_state_info_->BrowserStateIsAuthErrorAtIndex(cache_index_));

  // Set auth error.
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_TRUE(
      browser_state_info_->BrowserStateIsAuthErrorAtIndex(cache_index_));

  // Remove auth error.
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.account_id, GoogleServiceAuthError::AuthErrorNone());
  EXPECT_FALSE(
      browser_state_info_->BrowserStateIsAuthErrorAtIndex(cache_index_));
}
