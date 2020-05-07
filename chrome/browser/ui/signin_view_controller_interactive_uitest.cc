// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/callback.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/reauth_result.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/signin_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/base/signin_metrics.h"
#include "content/public/test/browser_test.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

class SignInViewControllerBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    // Many hotkeys are defined by the main menu. The value of these hotkeys
    // depends on the focused window. We must focus the browser window. This is
    // also why this test must be an interactive_ui_test rather than a browser
    // test.
    ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
        browser()->window()->GetNativeWindow()));
  }

  signin::IdentityManager* GetIdentityManager() {
    return IdentityManagerFactory::GetForProfile(browser()->profile());
  }
};

IN_PROC_BROWSER_TEST_F(SignInViewControllerBrowserTest, Accelerators) {
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  browser()->signin_view_controller()->ShowSignin(
      profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN,
      signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);

  ui_test_utils::TabAddedWaiter wait_for_new_tab(browser());
// Press Ctrl/Cmd+T, which will open a new tab.
#if defined(OS_MACOSX)
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_T, /*control=*/false, /*shift=*/false, /*alt=*/false,
      /*command=*/true));
#else
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_T, /*control=*/true, /*shift=*/false, /*alt=*/false,
      /*command=*/false));
#endif

  wait_for_new_tab.Wait();

  EXPECT_EQ(2, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(SignInViewControllerBrowserTest, AbortOngoingReauth) {
  base::MockCallback<base::OnceCallback<void(signin::ReauthResult)>>
      reauth_callback;
  signin::MakePrimaryAccountAvailable(GetIdentityManager(), "alice@gmail.com");
  std::unique_ptr<SigninViewController::ReauthAbortHandle> abort_handle =
      browser()->signin_view_controller()->ShowReauthPrompt(
          GetIdentityManager()->GetPrimaryAccountId(), reauth_callback.Get());
  EXPECT_CALL(reauth_callback, Run(signin::ReauthResult::kCancelled));
  abort_handle.reset();
}
