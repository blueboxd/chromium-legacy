// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/signin/e2e_tests/test_accounts_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

using signin::test::TestAccount;
using signin::test::TestAccountsUtil;

base::FilePath::StringPieceType kTestAccountFilePath = FILE_PATH_LITERAL(
    "chrome/browser/internal/resources/signin/test_accounts.json");

const char* kRunLiveTestFlag = "run-live-tests";

class DemoSignInTest : public InProcessBrowserTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    // Whitelists a bunch of hosts.
    host_resolver()->AllowDirectLookup("*.google.com");
    host_resolver()->AllowDirectLookup("*.geotrust.com");
    host_resolver()->AllowDirectLookup("*.gstatic.com");
    host_resolver()->AllowDirectLookup("*.googleapis.com");

    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUp() override {
    // Only run live tests when specified.
    auto* cmd_line = base::CommandLine::ForCurrentProcess();
    if (!cmd_line->HasSwitch(kRunLiveTestFlag)) {
      GTEST_SKIP();
    }

    base::FilePath root_path;
    base::PathService::Get(base::BasePathKey::DIR_SOURCE_ROOT, &root_path);
    base::FilePath config_path =
        base::MakeAbsoluteFilePath(root_path.Append(kTestAccountFilePath));
    test_accounts_.Init(config_path);
    InProcessBrowserTest::SetUp();
  }

  const TestAccountsUtil* GetTestAccountsUtil() const {
    return &test_accounts_;
  }

 private:
  TestAccountsUtil test_accounts_;
};

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
