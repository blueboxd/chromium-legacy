// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/web_applications/system_web_app_integration_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/system_web_app_manager_browsertest.h"
#include "chromeos/components/help_app_ui/url_constants.h"
#include "chromeos/components/web_applications/test/sandboxed_web_ui_test_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

using HelpAppIntegrationTest = SystemWebAppIntegrationTest;

// Test that the Help App installs and launches correctly. Runs some spot
// checks on the manifest.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2) {
  const GURL url(chromeos::kChromeUIHelpAppURL);
  EXPECT_NO_FATAL_FAILURE(
      ExpectSystemWebAppValid(web_app::SystemAppType::HELP, url, "Explore"));
}

// Test that the Help App is searchable by additional strings.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2SearchInLauncher) {
  WaitForTestSystemAppInstall();
  EXPECT_EQ(
      std::vector<std::string>({"Get Help", "Perks", "Offers"}),
      GetManager().GetAdditionalSearchTerms(web_app::SystemAppType::HELP));
}

// Test that the Help App has a minimum window size of 600x320.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2MinWindowSize) {
  WaitForTestSystemAppInstall();
  auto app_id = LaunchParamsForApp(web_app::SystemAppType::HELP).app_id;
  EXPECT_EQ(GetManager().GetMinimumWindowSize(app_id), gfx::Size(600, 320));
}

// Test that the Help App has a default size of 960x600 and is in the center of
// the screen.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2DefaultWindowBounds) {
  WaitForTestSystemAppInstall();
  Browser* browser;
  LaunchApp(web_app::SystemAppType::HELP, &browser);
  gfx::Rect work_area =
      display::Screen::GetScreen()->GetDisplayForNewWindows().work_area();
  int x = (work_area.width() - 960) / 2;
  int y = (work_area.height() - 600) / 2;
  EXPECT_EQ(browser->window()->GetBounds(), gfx::Rect(x, y, 960, 600));
}

// Test that the Help App logs metric when launching the app using the
// AppServiceProxy.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2AppServiceMetrics) {
  WaitForTestSystemAppInstall();
  base::HistogramTester histogram_tester;

  // Not using LaunchApp(..) here as that doesn't use the AppServiceProxy, so
  // doesn't log the metric that we are testing.
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
  proxy->Launch(
      *GetManager().GetAppIdForSystemApp(web_app::SystemAppType::HELP),
      ui::EventFlags::EF_NONE, apps::mojom::LaunchSource::kFromKeyboard,
      display::kDefaultDisplayId);

  // The HELP app is 18, see DefaultAppName in
  // src/chrome/browser/apps/app_service/app_service_metrics.cc
  histogram_tester.ExpectUniqueSample("Apps.DefaultAppLaunch.FromKeyboard", 18,
                                      1);
}

// Test that the Help App can log metrics in the untrusted frame.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2InAppMetrics) {
  WaitForTestSystemAppInstall();
  content::WebContents* web_contents = LaunchApp(web_app::SystemAppType::HELP);

  base::UserActionTester user_action_tester;

  constexpr char kScript[] = R"(
    chrome.metricsPrivate.recordUserAction("Discover.Help.TabClicked");
  )";

  EXPECT_EQ(0, user_action_tester.GetActionCount("Discover.Help.TabClicked"));
  EXPECT_EQ(nullptr,
            SandboxedWebUiAppTestBase::EvalJsInAppFrame(web_contents, kScript));
  EXPECT_EQ(1, user_action_tester.GetActionCount("Discover.Help.TabClicked"));
}

// Test that the Help App shortcut doesn't crash an incognito browser.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2Incognito) {
  WaitForTestSystemAppInstall();
  Browser* incognito_browser = CreateIncognitoBrowser();
  EXPECT_NO_FATAL_FAILURE(
      chrome::ShowHelp(incognito_browser, chrome::HELP_SOURCE_KEYBOARD));
}

// Test that the Help App does a navigation on launch even when it was already
// open with the same URL.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2NavigateOnRelaunch) {
  WaitForTestSystemAppInstall();

  // There should initially be a single browser window.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  Browser* browser;
  content::WebContents* web_contents =
      LaunchApp(web_app::SystemAppType::HELP, &browser);

  // There should be two browser windows, one regular and one for the newly
  // opened app.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  content::TestNavigationObserver navigation_observer(web_contents);
  LaunchAppWithoutWaiting(web_app::SystemAppType::HELP);
  // If no navigation happens, then this test will time out due to the wait.
  navigation_observer.Wait();

  // LaunchApp should navigate the existing window and not open any new windows.
  EXPECT_EQ(browser, chrome::FindLastActive());
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
}

// Test direct navigation to a subpage.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2DirectNavigation) {
  WaitForTestSystemAppInstall();
  auto params = LaunchParamsForApp(web_app::SystemAppType::HELP);
  params.override_url = GURL("chrome://help-app/help/");

  content::WebContents* web_contents = LaunchApp(params);

  // The inner frame should have the same pathname as the launch URL.
  EXPECT_EQ("chrome-untrusted://help-app/help/",
            SandboxedWebUiAppTestBase::EvalJsInAppFrame(
                web_contents, "window.location.href"));
}

INSTANTIATE_TEST_SUITE_P(All,
                         HelpAppIntegrationTest,
                         ::testing::Values(web_app::ProviderType::kBookmarkApps,
                                           web_app::ProviderType::kWebApps),
                         web_app::ProviderTypeParamToString);
