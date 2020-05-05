// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/process_dice_header_delegate_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ProcessDiceHeaderDelegateImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ProcessDiceHeaderDelegateImplTest()
      : enable_sync_called_(false),
        show_error_called_(false),
        account_id_("12345"),
        email_("foo@bar.com"),
        auth_error_(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS) {}

  ~ProcessDiceHeaderDelegateImplTest() override {}

  // Creates a ProcessDiceHeaderDelegateImpl instance.
  std::unique_ptr<ProcessDiceHeaderDelegateImpl> CreateDelegate(
      bool is_sync_signin_tab) {
    return std::make_unique<ProcessDiceHeaderDelegateImpl>(
        web_contents(), identity_test_environment_.identity_manager(),
        /*interceptor=*/nullptr, is_sync_signin_tab,
        base::BindOnce(&ProcessDiceHeaderDelegateImplTest::StartSyncCallback,
                       base::Unretained(this)),
        base::BindOnce(
            &ProcessDiceHeaderDelegateImplTest::ShowSigninErrorCallback,
            base::Unretained(this)));
  }

  // Callback for the ProcessDiceHeaderDelegateImpl.
  void StartSyncCallback(content::WebContents* contents,
                         const CoreAccountId& account_id) {
    EXPECT_EQ(web_contents(), contents);
    EXPECT_EQ(account_id_, account_id);
    enable_sync_called_ = true;
  }

  // Callback for the ProcessDiceHeaderDelegateImpl.
  void ShowSigninErrorCallback(content::WebContents* contents,
                               const std::string& error_message,
                               const std::string& email) {
    EXPECT_EQ(web_contents(), contents);
    EXPECT_EQ(auth_error_.ToString(), error_message);
    EXPECT_EQ(email_, email);
    show_error_called_ = true;
  }

  signin::IdentityTestEnvironment identity_test_environment_;

  bool enable_sync_called_;
  bool show_error_called_;
  CoreAccountId account_id_;
  std::string email_;
  GoogleServiceAuthError auth_error_;
};

// Check that sync is enabled if the tab is closed during signin.
TEST_F(ProcessDiceHeaderDelegateImplTest, CloseTabWhileStartingSync) {
  // Setup the test.
  GURL kSigninURL = GURL("https://accounts.google.com");
  NavigateAndCommit(kSigninURL);
  ASSERT_EQ(kSigninURL, web_contents()->GetVisibleURL());
  std::unique_ptr<ProcessDiceHeaderDelegateImpl> delegate =
      CreateDelegate(true);

  // Close the tab.
  DeleteContents();

  // Check expectations.
  delegate->EnableSync(account_id_);
  EXPECT_TRUE(enable_sync_called_);
  EXPECT_FALSE(show_error_called_);
}

// Check that the error is still shown if the tab is closed before the error is
// received.
TEST_F(ProcessDiceHeaderDelegateImplTest, CloseTabWhileFailingSignin) {
  // Setup the test.
  GURL kSigninURL = GURL("https://accounts.google.com");
  NavigateAndCommit(kSigninURL);
  ASSERT_EQ(kSigninURL, web_contents()->GetVisibleURL());
  std::unique_ptr<ProcessDiceHeaderDelegateImpl> delegate =
      CreateDelegate(true);

  // Close the tab.
  DeleteContents();

  // Check expectations.
  delegate->HandleTokenExchangeFailure(email_, auth_error_);
  EXPECT_FALSE(enable_sync_called_);
  EXPECT_TRUE(show_error_called_);
}

struct TestConfiguration {
  // Test setup.
  bool signed_in;   // User was already signed in at the start of the flow.
  bool signin_tab;  // The tab is marked as a Sync signin tab.

  // Test expectations.
  bool callback_called;  // The relevant callback was called.
  bool show_ntp;         // The NTP was shown.
};

TestConfiguration kEnableSyncTestCases[] = {
    // clang-format off
    // signed_in | signin_tab | callback_called | show_ntp
    {  false,      false,       false,            false},
    {  false,      true,        true,             true},
    {  true,       false,       false,            false},
    {  true,       true,        false,            false},
    // clang-format on
};

// Parameterized version of ProcessDiceHeaderDelegateImplTest.
class ProcessDiceHeaderDelegateImplTestEnableSync
    : public ProcessDiceHeaderDelegateImplTest,
      public ::testing::WithParamInterface<TestConfiguration> {};

// Test the EnableSync() method in all configurations.
TEST_P(ProcessDiceHeaderDelegateImplTestEnableSync, EnableSync) {
  // Setup the test.
  GURL kSigninURL = GURL("https://accounts.google.com");
  NavigateAndCommit(kSigninURL);
  ASSERT_EQ(kSigninURL, web_contents()->GetVisibleURL());
  if (GetParam().signed_in)
    identity_test_environment_.SetPrimaryAccount(email_);
  std::unique_ptr<ProcessDiceHeaderDelegateImpl> delegate =
      CreateDelegate(GetParam().signin_tab);

  // Check expectations.
  delegate->EnableSync(account_id_);
  EXPECT_EQ(GetParam().callback_called, enable_sync_called_);
  GURL expected_url =
      GetParam().show_ntp ? GURL(chrome::kChromeSearchLocalNtpUrl) : kSigninURL;
  EXPECT_EQ(expected_url, web_contents()->GetVisibleURL());
  EXPECT_FALSE(show_error_called_);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ProcessDiceHeaderDelegateImplTestEnableSync,
                         ::testing::ValuesIn(kEnableSyncTestCases));

TestConfiguration kHandleTokenExchangeFailureTestCases[] = {
    // clang-format off
    // signed_in | signin_tab | callback_called | show_ntp
    {  false,      false,       true,             false},
    {  false,      true,        true,             true},
    {  true,       false,       true,             false},
    {  true,       true,        true,             false},
    // clang-format on
};

// Parameterized version of ProcessDiceHeaderDelegateImplTest.
class ProcessDiceHeaderDelegateImplTestHandleTokenExchangeFailure
    : public ProcessDiceHeaderDelegateImplTest,
      public ::testing::WithParamInterface<TestConfiguration> {};

// Test the HandleTokenExchangeFailure() method in all configurations.
TEST_P(ProcessDiceHeaderDelegateImplTestHandleTokenExchangeFailure,
       HandleTokenExchangeFailure) {
  // Setup the test.
  GURL kSigninURL = GURL("https://accounts.google.com");
  NavigateAndCommit(kSigninURL);
  ASSERT_EQ(kSigninURL, web_contents()->GetVisibleURL());
  if (GetParam().signed_in)
    identity_test_environment_.SetPrimaryAccount(email_);
  std::unique_ptr<ProcessDiceHeaderDelegateImpl> delegate =
      CreateDelegate(GetParam().signin_tab);

  // Check expectations.
  delegate->HandleTokenExchangeFailure(email_, auth_error_);
  EXPECT_FALSE(enable_sync_called_);
  EXPECT_EQ(GetParam().callback_called, show_error_called_);
  GURL expected_url =
      GetParam().show_ntp ? GURL(chrome::kChromeSearchLocalNtpUrl) : kSigninURL;
  EXPECT_EQ(expected_url, web_contents()->GetVisibleURL());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ProcessDiceHeaderDelegateImplTestHandleTokenExchangeFailure,
    ::testing::ValuesIn(kHandleTokenExchangeFailureTestCases));

}  // namespace
