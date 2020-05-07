// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/custom_tab_bar_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/theme_change_waiter.h"

namespace web_app {

class WebAppTabStripBrowserTest : public InProcessBrowserTest {
 public:
  WebAppTabStripBrowserTest() = default;
  ~WebAppTabStripBrowserTest() override = default;

  void SetUp() override {
    features_.InitWithFeatures({features::kDesktopPWAsTabStrip}, {});
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest,
                       CustomTabBarUpdateOnTabSwitch) {
  Profile* profile = browser()->profile();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL app_url = embedded_test_server()->GetURL("/web_apps/basic.html");

  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->app_url = app_url;
  web_app_info->scope = embedded_test_server()->GetURL("/web_apps");
  web_app_info->title = base::ASCIIToUTF16("Test app");
  web_app_info->open_as_window = true;
  AppId app_id = InstallWebApp(profile, std::move(web_app_info));
  WebAppProviderBase::GetProviderBase(profile)
      ->registry_controller()
      .SetExperimentalTabbedWindowMode(app_id, true);

  Browser* app_browser = LaunchWebAppBrowser(profile, app_id);
  CustomTabBarView* custom_tab_bar =
      BrowserView::GetBrowserViewForBrowser(app_browser)
          ->toolbar()
          ->custom_tab_bar();
  EXPECT_FALSE(custom_tab_bar->GetVisible());

  // Add second tab.
  chrome::NewTab(app_browser);
  ASSERT_EQ(app_browser->tab_strip_model()->count(), 2);

  // Navigate tab out of scope, custom tab bar should appear.
  GURL out_of_scope_url =
      embedded_test_server()->GetURL("/banners/theme-color.html");
  ASSERT_TRUE(content::NavigateToURL(
      app_browser->tab_strip_model()->GetActiveWebContents(),
      out_of_scope_url));
  EXPECT_EQ(
      app_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      out_of_scope_url);
  EXPECT_TRUE(custom_tab_bar->GetVisible());

  // Custom tab bar should go away for in scope tab.
  chrome::SelectNextTab(app_browser);
  EXPECT_EQ(
      app_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      app_url);
  EXPECT_FALSE(custom_tab_bar->GetVisible());

  // Custom tab bar should return for out of scope tab.
  chrome::SelectNextTab(app_browser);
  EXPECT_EQ(
      app_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      out_of_scope_url);
  EXPECT_TRUE(custom_tab_bar->GetVisible());
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, TabThemeColor) {
  Profile* profile = browser()->profile();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL app_url = embedded_test_server()->GetURL("/banners/theme-color.html");

  // Install and launch theme color test PWA.
  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->app_url = app_url;
  web_app_info->scope = app_url.GetWithoutFilename();
  web_app_info->title = base::ASCIIToUTF16("Test app");
  web_app_info->open_as_window = true;
  AppId app_id = InstallWebApp(profile, std::move(web_app_info));
  WebAppProviderBase::GetProviderBase(profile)
      ->registry_controller()
      .SetExperimentalTabbedWindowMode(app_id, true);
  Browser* app_browser = LaunchWebAppBrowser(profile, app_id);
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  BrowserView* app_browser_view =
      BrowserView::GetBrowserViewForBrowser(app_browser);

  // Ensure HTML document has loaded as we are going to execute JS in it.
  content::AwaitDocumentOnLoadCompleted(web_contents);

  auto GetTabColor = [app_browser_view]() -> SkColor {
    return app_browser_view->tabstrip()->GetTabBackgroundColor(
        TabActive::kActive, BrowserFrameActiveState::kActive);
  };

  // Set theme color to black and read tab background color.
  SkColor initial_tab_color = SK_ColorTRANSPARENT;
  {
    content::ThemeChangeWaiter waiter(web_contents);
    EXPECT_TRUE(content::ExecJs(web_contents,
                                "document.getElementById('theme-color')."
                                "setAttribute('content', 'black')"));
    waiter.Wait();
    EXPECT_EQ(app_browser->app_controller()->GetThemeColor().value(),
              SK_ColorBLACK);
    initial_tab_color = GetTabColor();
    EXPECT_NE(initial_tab_color, SK_ColorTRANSPARENT);
    EXPECT_EQ(initial_tab_color, SK_ColorBLACK);
  }

  // Update theme color to cyan and check that the tab color matches.
  {
    content::ThemeChangeWaiter waiter(web_contents);
    EXPECT_TRUE(content::ExecJs(web_contents,
                                "document.getElementById('theme-color')."
                                "setAttribute('content', 'cyan')"));
    waiter.Wait();
    EXPECT_EQ(app_browser->app_controller()->GetThemeColor().value(),
              SK_ColorCYAN);
    SkColor tab_color = GetTabColor();
    EXPECT_NE(tab_color, initial_tab_color);
    EXPECT_EQ(tab_color, SK_ColorCYAN);
  }
}

}  // namespace web_app
