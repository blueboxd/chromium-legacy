// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/signin/e2e_tests/live_test.h"
#include "chrome/browser/signin/e2e_tests/test_accounts_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

using signin::test::TestAccount;
using signin::test::TestAccountsUtil;

class DemoSignInTest : public signin::test::LiveTest {};

IN_PROC_BROWSER_TEST_F(DemoSignInTest, SimpleSignInFlow) {
  // Always disable animation for stability.
  ui::ScopedAnimationDurationScaleMode disable_animation(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  GURL url("chrome://settings");
  AddTabAtIndex(0, url, ui::PageTransition::PAGE_TRANSITION_TYPED);
  auto* settings_tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScript(
      settings_tab,
      "testElement = document.createElement('settings-sync-account-control');"
      "document.body.appendChild(testElement);"
      "testElement.$$('#sign-in').click();"));
  TestAccount ta;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", ta));
  login_ui_test_utils::ExecuteJsToSigninInSigninFrame(browser(), ta.user,
                                                      ta.password);
  login_ui_test_utils::DismissSyncConfirmationDialog(
      browser(), base::TimeDelta::FromSeconds(3));
  GURL signin("chrome://signin-internals");
  AddTabAtIndex(0, signin, ui::PageTransition::PAGE_TRANSITION_TYPED);
  auto* signin_tab = browser()->tab_strip_model()->GetActiveWebContents();
  std::string query_str = base::StringPrintf(
      "document.body.innerHTML.indexOf('%s');", ta.user.c_str());
  EXPECT_GT(content::EvalJs(signin_tab, query_str).ExtractInt(), 0);
}
