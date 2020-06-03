// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/reauth_result.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/signin_reauth_view_controller.h"
#include "chrome/browser/ui/signin_view_controller.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/core_account_id.h"

// Browser tests for SigninReauthViewController.
class SigninReauthViewControllerBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    account_id_ = signin::SetUnconsentedPrimaryAccount(identity_manager(),
                                                       "alice@gmail.com")
                      .account_id;
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(browser()->profile());
  }

  SigninReauthViewController* signin_reauth_view_controller() {
    SigninViewController* signin_view_controller =
        browser()->signin_view_controller();
    DCHECK(signin_view_controller->ShowsModalDialog());
    return static_cast<SigninReauthViewController*>(
        signin_view_controller->GetModalDialogDelegateForTesting());
  }

  CoreAccountId account_id() { return account_id_; }

 private:
  CoreAccountId account_id_;
};

// Tests that the abort handle cancels an ongoing reauth flow.
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest,
                       AbortReauthDialog_AbortHandle) {
  base::MockCallback<base::OnceCallback<void(signin::ReauthResult)>>
      reauth_callback;
  std::unique_ptr<SigninViewController::ReauthAbortHandle> abort_handle =
      browser()->signin_view_controller()->ShowReauthPrompt(
          account_id(), reauth_callback.Get());
  EXPECT_CALL(reauth_callback, Run(signin::ReauthResult::kCancelled));
  abort_handle.reset();
}

// Tests canceling the reauth dialog through CloseModalSignin().
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest,
                       AbortReauthDialog_CloseModalSignin) {
  base::MockCallback<base::OnceCallback<void(signin::ReauthResult)>>
      reauth_callback;
  std::unique_ptr<SigninViewController::ReauthAbortHandle> abort_handle =
      browser()->signin_view_controller()->ShowReauthPrompt(
          account_id(), reauth_callback.Get());
  EXPECT_CALL(reauth_callback, Run(signin::ReauthResult::kCancelled));
  browser()->signin_view_controller()->CloseModalSignin();
}

// Tests closing the reauth dialog through by clicking on the close button (the
// X).
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest,
                       CloseReauthDialog) {
  base::MockCallback<base::OnceCallback<void(signin::ReauthResult)>>
      reauth_callback;
  std::unique_ptr<SigninViewController::ReauthAbortHandle> abort_handle =
      browser()->signin_view_controller()->ShowReauthPrompt(
          account_id(), reauth_callback.Get());
  EXPECT_CALL(reauth_callback, Run(signin::ReauthResult::kDismissedByUser));
  // The test cannot depend on Views implementation so it simulates clicking on
  // the close button through calling the close event.
  signin_reauth_view_controller()->OnModalSigninClosed();
}

// Tests clicking on the confirm button in the reauth dialog.
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest,
                       ConfirmReauthDialog) {
  base::Optional<signin::ReauthResult> reauth_result;
  base::RunLoop run_loop;
  std::unique_ptr<SigninViewController::ReauthAbortHandle> abort_handle =
      browser()->signin_view_controller()->ShowReauthPrompt(
          account_id(),
          base::BindLambdaForTesting([&](signin::ReauthResult result) {
            reauth_result = result;
            run_loop.Quit();
          }));
  ASSERT_TRUE(login_ui_test_utils::ConfirmReauthConfirmationDialog(
      browser(), base::TimeDelta::FromSeconds(30)));
  run_loop.Run();
  EXPECT_EQ(reauth_result, signin::ReauthResult::kSuccess);
}

// Tests clicking on the cancel button in the reauth dialog.
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest,
                       CancelReauthDialog) {
  base::Optional<signin::ReauthResult> reauth_result;
  base::RunLoop run_loop;
  std::unique_ptr<SigninViewController::ReauthAbortHandle> abort_handle =
      browser()->signin_view_controller()->ShowReauthPrompt(
          account_id(),
          base::BindLambdaForTesting([&](signin::ReauthResult result) {
            reauth_result = result;
            run_loop.Quit();
          }));
  ASSERT_TRUE(login_ui_test_utils::CancelReauthConfirmationDialog(
      browser(), base::TimeDelta::FromSeconds(30)));
  run_loop.Run();
  EXPECT_EQ(reauth_result, signin::ReauthResult::kDismissedByUser);
}
