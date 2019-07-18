// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_test_base.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::NavigationController;
using content::WebContents;

namespace {

static const char* kExampleHost = "www.example.com";
static const char* kExampleHost2 = "www.example2.com";

static const char* kIframeHost2 = "www.iframe2.com";

}  // namespace

class SupervisedUserNavigationThrottleTest : public SupervisedUserTestBase {
 protected:
  SupervisedUserNavigationThrottleTest() = default;
  ~SupervisedUserNavigationThrottleTest() override = default;

  void BlockHost(const std::string& host) {
    Profile* profile = GetPrimaryUserProfile();
    SupervisedUserSettingsService* settings_service =
        SupervisedUserSettingsServiceFactory::GetForKey(
            profile->GetProfileKey());
    auto dict = std::make_unique<base::DictionaryValue>();
    dict->SetKey(host, base::Value(false));
    settings_service->SetLocalSetting(
        supervised_users::kContentPackManualBehaviorHosts, std::move(dict));
  }

  bool IsInterstitialBeingShown(Browser* browser);

 private:
  void SetUpOnMainThread() override;
};

bool SupervisedUserNavigationThrottleTest::IsInterstitialBeingShown(
    Browser* browser) {
  WebContents* tab = browser->tab_strip_model()->GetActiveWebContents();
  base::string16 title;
  ui_test_utils::GetCurrentTabTitle(browser, &title);
  return tab->GetController().GetLastCommittedEntry()->GetPageType() ==
             content::PAGE_TYPE_ERROR &&
         title == base::ASCIIToUTF16("Site blocked");
}

void SupervisedUserNavigationThrottleTest::SetUpOnMainThread() {
  SupervisedUserTestBase::SetUpOnMainThread();

  // Resolve everything to localhost.
  host_resolver()->AddIPLiteralRule("*", "127.0.0.1", "localhost");

  ASSERT_TRUE(embedded_test_server()->Started());
}

// Tests that navigating to a blocked page simply fails if there is no
// SupervisedUserNavigationObserver.
IN_PROC_BROWSER_TEST_F(SupervisedUserNavigationThrottleTest,
                       NoNavigationObserverBlock) {
  LogInUser(LogInType::kChild);
  Profile* profile = GetPrimaryUserProfile();
  SupervisedUserSettingsService* supervised_user_settings_service =
      SupervisedUserSettingsServiceFactory::GetForKey(profile->GetProfileKey());
  supervised_user_settings_service->SetLocalSetting(
      supervised_users::kContentPackDefaultFilteringBehavior,
      std::unique_ptr<base::Value>(
          new base::Value(SupervisedUserURLFilter::BLOCK)));

  std::unique_ptr<WebContents> web_contents(
      WebContents::Create(WebContents::CreateParams(profile)));
  NavigationController& controller = web_contents->GetController();
  content::TestNavigationObserver observer(web_contents.get());
  controller.LoadURL(GURL("http://www.example.com"), content::Referrer(),
                     ui::PAGE_TRANSITION_TYPED, std::string());
  observer.Wait();
  content::NavigationEntry* entry = controller.GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, entry->GetPageType());
  EXPECT_FALSE(observer.last_navigation_succeeded());
}

IN_PROC_BROWSER_TEST_F(SupervisedUserNavigationThrottleTest,
                       BlockMainFrameWithInterstitial) {
  LogInUser(LogInType::kChild);

  BlockHost(kExampleHost2);

  GURL allowed_url = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/simple.html");
  ui_test_utils::NavigateToURL(browser(), allowed_url);
  EXPECT_FALSE(IsInterstitialBeingShown(browser()));

  GURL blocked_url = embedded_test_server()->GetURL(
      kExampleHost2, "/supervised_user/simple.html");
  ui_test_utils::NavigateToURL(browser(), blocked_url);
  EXPECT_TRUE(IsInterstitialBeingShown(browser()));
}

IN_PROC_BROWSER_TEST_F(SupervisedUserNavigationThrottleTest,
                       DontBlockSubFrame) {
  LogInUser(LogInType::kChild);

  BlockHost(kExampleHost2);
  BlockHost(kIframeHost2);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  GURL allowed_url_with_iframes = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/with_iframes.html");
  ui_test_utils::NavigateToURL(browser(), allowed_url_with_iframes);
  EXPECT_FALSE(IsInterstitialBeingShown(browser()));

  // Both iframes (from allowed host iframe1.com as well as from blocked host
  // iframe2.com) should be loaded normally, since we don't filter iframes
  // (yet) - see crbug.com/651115.
  bool loaded1 = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(tab, "loaded1()", &loaded1));
  EXPECT_TRUE(loaded1);
  bool loaded2 = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(tab, "loaded2()", &loaded2));
  EXPECT_TRUE(loaded2);
}

class SupervisedUserNavigationThrottleNotSupervisedTest
    : public SupervisedUserNavigationThrottleTest {
 protected:
  SupervisedUserNavigationThrottleNotSupervisedTest() = default;
  ~SupervisedUserNavigationThrottleNotSupervisedTest() override = default;
};

IN_PROC_BROWSER_TEST_F(SupervisedUserNavigationThrottleNotSupervisedTest,
                       DontBlock) {
  LogInUser(LogInType::kRegular);
  BlockHost(kExampleHost);

  GURL blocked_url = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/simple.html");
  ui_test_utils::NavigateToURL(browser(), blocked_url);
  // Even though the URL is marked as blocked, the load should go through, since
  // the user isn't supervised.
  EXPECT_FALSE(IsInterstitialBeingShown(browser()));
}
