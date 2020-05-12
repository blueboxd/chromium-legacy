// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "content/public/test/browser_test.h"

namespace web_app {

class WebAppLinkCapturingBrowserTest : public WebAppNavigationBrowserTest {
 public:
  WebAppLinkCapturingBrowserTest() = default;
  ~WebAppLinkCapturingBrowserTest() override = default;

  void SetUp() override {
    features_.InitWithFeatures({features::kDesktopPWAsTabStrip,
                                features::kDesktopPWAsTabStripLinkCapturing},
                               {});
    WebAppNavigationBrowserTest::SetUp();
  }

  WebAppProviderBase& provider() {
    auto* provider = WebAppProviderBase::GetProviderBase(profile());
    DCHECK(provider);
    return *provider;
  }

  void NavigateMainBrowser(const GURL& url) {
    ClickLinkAndWait(browser()->tab_strip_model()->GetActiveWebContents(), url,
                     LinkTarget::SELF, "");
  }

  void ExpectTabs(Browser* browser, std::vector<GURL> urls) {
    TabStripModel& tab_strip = *browser->tab_strip_model();
    ASSERT_EQ(static_cast<size_t>(tab_strip.count()), urls.size());
    for (int i = 0; i < tab_strip.count(); ++i) {
      SCOPED_TRACE(base::StringPrintf("is app browser: %d, tab index: %d",
                                      bool(browser->app_controller()), i));
      EXPECT_EQ(
          browser->tab_strip_model()->GetWebContentsAt(i)->GetVisibleURL(),
          urls[i]);
    }
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(WebAppLinkCapturingBrowserTest,
                       InScopeNavigationsCaptured) {
  GURL app_url("https://example.org/dir/start.html");
  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->app_url = app_url;
  web_app_info->open_as_window = true;
  AppId app_id = web_app::InstallWebApp(profile(), std::move(web_app_info));
  provider().registry_controller().SetExperimentalTabbedWindowMode(app_id,
                                                                   true);

  GURL about_blank("about:blank");
  GURL page1("https://example.org/dir/page2.html");
  GURL page2("https://example.org/dir/page2.html");
  GURL origin("https://example.org/");
  GURL out_of_scope("https://other-domain.org/");

  // In scope navigation should open app window.
  NavigateMainBrowser(page1);
  Browser* app_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_TRUE(AppBrowserController::IsForWebAppBrowser(app_browser, app_id));
  ExpectTabs(browser(), {about_blank});
  ExpectTabs(app_browser, {page1});

  // Another in scope navigation should open a new tab in the same app window.
  NavigateMainBrowser(page2);
  ExpectTabs(browser(), {about_blank});
  ExpectTabs(app_browser, {page1, page2});

  // Whole origin should count as in scope.
  NavigateMainBrowser(origin);
  ExpectTabs(browser(), {about_blank});
  ExpectTabs(app_browser, {page1, page2, origin});

  // Out of scope should behave as usual.
  NavigateMainBrowser(out_of_scope);
  ExpectTabs(browser(), {out_of_scope});
  ExpectTabs(app_browser, {page1, page2, origin});
}

}  // namespace web_app
