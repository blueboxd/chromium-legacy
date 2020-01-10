// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <set>

#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/child_accounts/child_user_service.h"
#include "chrome/browser/chromeos/child_accounts/child_user_service_factory.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_controller.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limits_whitelist_policy_test_utils.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/web_time_limit_enforcer.h"
#include "chrome/browser/chromeos/login/test/scoped_policy_update.h"
#include "chrome/browser/chromeos/login/test/user_policy_mixin.h"
#include "chrome/browser/chromeos/policy/user_policy_test_helper.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/supervised_user/logged_in_user_mixin.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {

constexpr char kExampleHost[] = "www.example.com";

class LoadFinishedWaiter : public content::WebContentsObserver {
 public:
  LoadFinishedWaiter(content::WebContents* web_contents, const GURL& url)
      : content::WebContentsObserver(web_contents), url_(url) {}

  ~LoadFinishedWaiter() override = default;

  // Delete copy constructor and copy assignment operator.
  LoadFinishedWaiter(const LoadFinishedWaiter&) = delete;
  LoadFinishedWaiter& operator=(const LoadFinishedWaiter&) = delete;

  void Wait() {
    if (did_finish_)
      return;
    run_loop_.Run();
  }

  // content::WebContentsObserver:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

 private:
  GURL url_;
  bool did_finish_ = false;
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
};

void LoadFinishedWaiter::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!render_frame_host->GetParent() &&
      render_frame_host->GetLastCommittedURL() == url_) {
    did_finish_ = true;
    run_loop_.Quit();
  }
}
}  // namespace

class WebTimeLimitEnforcerThrottleTest : public MixinBasedInProcessBrowserTest {
 protected:
  void SetUp() override;
  void TearDown() override;
  void SetUpOnMainThread() override;
  bool IsErrorPageBeingShownInWebContents(content::WebContents* tab);
  void WhitelistUrlRegx(const std::string& host);
  void WhitelistApp(const chromeos::app_time::AppId& app_id);
  void BlockWeb();
  chromeos::app_time::WebTimeLimitEnforcer* GetWebTimeLimitEnforcer();

 private:
  void UpdatePolicy();

  base::test::ScopedFeatureList scoped_feature_list_;

  chromeos::app_time::AppTimeLimitsWhitelistPolicyBuilder builder_;

  chromeos::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, chromeos::LoggedInUserMixin::LogInType::kChild,
      embedded_test_server(), this};
};

void WebTimeLimitEnforcerThrottleTest::SetUp() {
  scoped_feature_list_.InitWithFeatures(
      /* enabled_features */ {{features::kPerAppTimeLimits,
                               features::kWebTimeLimits}},
      /* disabled_features */ {{}});
  builder_.SetUp();
  MixinBasedInProcessBrowserTest::SetUp();
}

void WebTimeLimitEnforcerThrottleTest::TearDown() {
  builder_.Clear();
  MixinBasedInProcessBrowserTest::TearDown();
}

void WebTimeLimitEnforcerThrottleTest::SetUpOnMainThread() {
  MixinBasedInProcessBrowserTest::SetUpOnMainThread();

  ASSERT_TRUE(embedded_test_server()->Started());

  // Resolve everything to localhost.
  host_resolver()->AddIPLiteralRule("*", "127.0.0.1", "localhost");

  logged_in_user_mixin_.LogInUser(false /*issue_any_scope_token*/,
                                  true /*wait_for_active_session*/,
                                  true /*request_policy_update*/);
}

bool WebTimeLimitEnforcerThrottleTest::IsErrorPageBeingShownInWebContents(
    content::WebContents* tab) {
  const std::string command =
      "domAutomationController.send("
      "(document.getElementById('web-time-limit-block') != null)"
      "? (true) : (false));";

  bool value = false;
  content::ToRenderFrameHost target =
      content::ToRenderFrameHost(tab->GetMainFrame());
  EXPECT_TRUE(content::ExecuteScriptWithoutUserGestureAndExtractBool(
      target, command, &value));
  return value;
}

void WebTimeLimitEnforcerThrottleTest::WhitelistUrlRegx(
    const std::string& url) {
  builder_.AppendToWhitelistUrlList(url);
  UpdatePolicy();
}

void WebTimeLimitEnforcerThrottleTest::WhitelistApp(
    const chromeos::app_time::AppId& app_id) {
  builder_.AppendToWhitelistAppList(app_id);
  UpdatePolicy();
}

void WebTimeLimitEnforcerThrottleTest::BlockWeb() {
  GetWebTimeLimitEnforcer()->OnWebTimeLimitReached();
}

chromeos::app_time::WebTimeLimitEnforcer*
WebTimeLimitEnforcerThrottleTest::GetWebTimeLimitEnforcer() {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  chromeos::ChildUserService::TestApi child_user_service =
      chromeos::ChildUserService::TestApi(
          chromeos::ChildUserServiceFactory::GetForBrowserContext(
              browser_context));
  return child_user_service.web_time_enforcer();
}

void WebTimeLimitEnforcerThrottleTest::UpdatePolicy() {
  std::string policy_value;
  base::JSONWriter::Write(builder_.value(), &policy_value);

  logged_in_user_mixin_.GetUserPolicyMixin()
      ->RequestPolicyUpdate()
      ->policy_payload()
      ->mutable_perapptimelimitswhitelist()
      ->set_value(policy_value);

  const user_manager::UserManager* const user_manager =
      user_manager::UserManager::Get();

  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(
      user_manager->GetActiveUser());

  logged_in_user_mixin_.GetUserPolicyTestHelper()->RefreshPolicyAndWait(
      profile);
}

IN_PROC_BROWSER_TEST_F(WebTimeLimitEnforcerThrottleTest,
                       WebBlockedBeforeBrowser) {
  // Alright let's block the browser.
  GetWebTimeLimitEnforcer()->OnWebTimeLimitReached();
  GURL url = embedded_test_server()->GetURL(kExampleHost,
                                            "/supervised_user/simple.html");

  NavigateParams params(browser(), url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);

  EXPECT_TRUE(IsErrorPageBeingShownInWebContents(
      params.navigated_or_inserted_contents));
}

IN_PROC_BROWSER_TEST_F(WebTimeLimitEnforcerThrottleTest,
                       WebBlockedAfterBrowser) {
  GURL url = embedded_test_server()->GetURL(kExampleHost,
                                            "/supervised_user/simple.html");
  NavigateParams params(browser(), url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);

  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);
  auto* web_contents = params.navigated_or_inserted_contents;

  // We don't expect an time limit block page to show yet.
  EXPECT_FALSE(IsErrorPageBeingShownInWebContents(web_contents));

  LoadFinishedWaiter waiter(web_contents, url);

  GetWebTimeLimitEnforcer()->OnWebTimeLimitReached();

  waiter.Wait();

  EXPECT_TRUE(IsErrorPageBeingShownInWebContents(web_contents));
}

IN_PROC_BROWSER_TEST_F(WebTimeLimitEnforcerThrottleTest,
                       WebUnblockedAfterBlocked) {
  GURL url = embedded_test_server()->GetURL(kExampleHost,
                                            "/supervised_user/simple.html");

  // Alright let's block the browser.
  GetWebTimeLimitEnforcer()->OnWebTimeLimitReached();
  NavigateParams params(browser(), url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);
  auto* web_contents = params.navigated_or_inserted_contents;

  EXPECT_TRUE(IsErrorPageBeingShownInWebContents(web_contents));

  LoadFinishedWaiter waiter(web_contents, url);

  GetWebTimeLimitEnforcer()->OnWebTimeLimitEnded();
  waiter.Wait();

  EXPECT_FALSE(IsErrorPageBeingShownInWebContents(web_contents));
}

IN_PROC_BROWSER_TEST_F(WebTimeLimitEnforcerThrottleTest,
                       WhitelistedURLNotBlocked) {
  GURL url = embedded_test_server()->GetURL(kExampleHost,
                                            "/supervised_user/simple.html");
  WhitelistUrlRegx(kExampleHost);

  // Alright let's block the browser.
  GetWebTimeLimitEnforcer()->OnWebTimeLimitReached();
  NavigateParams params(browser(), url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);
  auto* web_contents = params.navigated_or_inserted_contents;

  EXPECT_FALSE(IsErrorPageBeingShownInWebContents(web_contents));
}

IN_PROC_BROWSER_TEST_F(WebTimeLimitEnforcerThrottleTest,
                       BlockedURLAddedToWhitelist) {
  GURL url = embedded_test_server()->GetURL(kExampleHost,
                                            "/supervised_user/simple.html");

  // Alright let's block the browser.
  GetWebTimeLimitEnforcer()->OnWebTimeLimitReached();
  NavigateParams params(browser(), url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);
  auto* web_contents = params.navigated_or_inserted_contents;

  EXPECT_TRUE(IsErrorPageBeingShownInWebContents(web_contents));

  LoadFinishedWaiter waiter(web_contents, url);

  WhitelistUrlRegx(kExampleHost);
  waiter.Wait();

  EXPECT_FALSE(IsErrorPageBeingShownInWebContents(web_contents));
}

IN_PROC_BROWSER_TEST_F(WebTimeLimitEnforcerThrottleTest,
                       WhitelistedSchemesNotBlockedHttp) {
  GURL url = embedded_test_server()->GetURL(kExampleHost,
                                            "/supervised_user/simple.html");

  GetWebTimeLimitEnforcer()->OnWebTimeLimitReached();
  NavigateParams params(browser(), url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);
  auto* web_contents = params.navigated_or_inserted_contents;
  EXPECT_TRUE(IsErrorPageBeingShownInWebContents(web_contents));

  // Whitelist the http scheme and ensure that the page is not
  // blocked
  LoadFinishedWaiter waiter(web_contents, url);
  WhitelistUrlRegx("http://*");
  waiter.Wait();
  EXPECT_FALSE(IsErrorPageBeingShownInWebContents(web_contents));
}

IN_PROC_BROWSER_TEST_F(WebTimeLimitEnforcerThrottleTest,
                       WhitelistedSchemesNotBlockedChrome) {
  GURL url = GURL("chrome://version");

  GetWebTimeLimitEnforcer()->OnWebTimeLimitReached();
  NavigateParams params(browser(), url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);
  auto* web_contents = params.navigated_or_inserted_contents;
  EXPECT_TRUE(IsErrorPageBeingShownInWebContents(web_contents));

  // Whitelist the chrome scheme and ensure that the page is not
  // blocked.
  LoadFinishedWaiter waiter(web_contents, url);
  WhitelistUrlRegx("chrome://*");
  waiter.Wait();
  EXPECT_FALSE(IsErrorPageBeingShownInWebContents(web_contents));
}

// TODO(yilkal): Add WhitelistedSchemeNotBlocked test for  chrome://settings
