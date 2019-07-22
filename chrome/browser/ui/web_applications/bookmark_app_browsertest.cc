// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_metrics.h"

#include <bitset>

#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/web_app_metrics.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

namespace {

void NavigateToURLAndWait(Browser* browser, const GURL& url) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  content::TestNavigationObserver observer(
      web_contents, content::MessageLoopRunner::QuitMode::DEFERRED);
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);
  observer.WaitForNavigationFinished();
}

// TODO(loyso): Merge this with PendingBookmarkAppManagerBrowserTest's
// implementation in some test_support library.
web_app::ExternalInstallOptions CreateInstallOptions(const GURL& url) {
  web_app::ExternalInstallOptions install_options(
      url, web_app::LaunchContainer::kWindow,
      web_app::ExternalInstallSource::kInternalDefault);
  // Avoid creating real shortcuts in tests.
  install_options.add_to_applications_menu = false;
  install_options.add_to_desktop = false;
  install_options.add_to_quick_launch_bar = false;
  return install_options;
}

GURL GetUrlForSuffix(const std::string& prefix, int suffix) {
  return GURL(prefix + base::NumberToString(suffix) + ".com/");
}

// Must be zero-based as this will be stored in a bitset.
enum HistogramIndex {
  kHistogramInTab = 0,
  kHistogramInWindow,
  kHistogramDefaultInstalled_InTab,
  kHistogramDefaultInstalled_InWindow,
  kHistogramUserInstalled_InTab,
  kHistogramUserInstalled_InWindow,
  kHistogramUserInstalled_FromInstallButton_InTab,
  kHistogramUserInstalled_FromInstallButton_InWindow,
  kHistogramUserInstalled_FromCreateShortcutButton_InTab,
  kHistogramUserInstalled_FromCreateShortcutButton_InWindow,
  kHistogramMoreThanThreeUserInstalledApps,
  kHistogramUpToThreeUserInstalledApps,
  kHistogramNoUserInstalledApps,
  kHistogramMaxValue
};

// The order (indices) must match HistogramIndex enum above:
const char* kHistogramNames[] = {
    "WebApp.Engagement.InTab",
    "WebApp.Engagement.InWindow",
    "WebApp.Engagement.DefaultInstalled.InTab",
    "WebApp.Engagement.DefaultInstalled.InWindow",
    "WebApp.Engagement.UserInstalled.InTab",
    "WebApp.Engagement.UserInstalled.InWindow",
    "WebApp.Engagement.UserInstalled.FromInstallButton.InTab",
    "WebApp.Engagement.UserInstalled.FromInstallButton.InWindow",
    "WebApp.Engagement.UserInstalled.FromCreateShortcutButton.InTab",
    "WebApp.Engagement.UserInstalled.FromCreateShortcutButton.InWindow",
    "WebApp.Engagement.MoreThanThreeUserInstalledApps",
    "WebApp.Engagement.UpToThreeUserInstalledApps",
    "WebApp.Engagement.NoUserInstalledApps"};

const char* HistogramEnumIndexToStr(int histogram_index) {
  DCHECK_GE(histogram_index, 0);
  DCHECK_LT(histogram_index, kHistogramMaxValue);
  return kHistogramNames[histogram_index];
}

using Histograms = std::bitset<kHistogramMaxValue>;

void ExpectUniqueSamples(const base::HistogramTester& tester,
                         const Histograms& histograms_mask,
                         SiteEngagementService::EngagementType type,
                         base::HistogramBase::Count count) {
  for (int h = 0; h < kHistogramMaxValue; ++h) {
    if (histograms_mask[h]) {
      const char* histogram_name = HistogramEnumIndexToStr(h);
      tester.ExpectUniqueSample(histogram_name, type, count);
    }
  }
}

void ExpectBucketCounts(const base::HistogramTester& tester,
                        const Histograms& histograms_mask,
                        SiteEngagementService::EngagementType type,
                        base::HistogramBase::Count count) {
  for (int h = 0; h < kHistogramMaxValue; ++h) {
    if (histograms_mask[h]) {
      const char* histogram_name = HistogramEnumIndexToStr(h);
      tester.ExpectBucketCount(histogram_name, type, count);
    }
  }
}

void ExpectTotalCounts(const base::HistogramTester& tester,
                       const Histograms& histograms_mask,
                       base::HistogramBase::Count count) {
  for (int h = 0; h < kHistogramMaxValue; ++h) {
    if (histograms_mask[h]) {
      const char* histogram_name = HistogramEnumIndexToStr(h);
      tester.ExpectTotalCount(histogram_name, count);
    }
  }
}

}  // namespace

class BookmarkAppTest : public extensions::ExtensionBrowserTest {
 public:
  BookmarkAppTest() = default;
  ~BookmarkAppTest() override = default;

  void TestEngagementEventWebAppLaunch(const base::HistogramTester& tester,
                                       const Histograms& histograms) {
    ExpectUniqueSamples(
        tester, histograms,
        SiteEngagementService::ENGAGEMENT_WEBAPP_SHORTCUT_LAUNCH, 1);
    ExpectTotalCounts(tester, ~histograms, 0);
  }

  // Test some other engagement events by directly calling into
  // SiteEngagementService.
  void TestEngagementEventsAfterLaunch(const Histograms& histograms,
                                       Browser* browser) {
    base::HistogramTester tester;

    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    SiteEngagementService* site_engagement_service =
        SiteEngagementService::Get(browser->profile());

    // Simulate 4 events of various types.
    site_engagement_service->HandleMediaPlaying(web_contents, false);
    site_engagement_service->HandleMediaPlaying(web_contents, true);
    site_engagement_service->HandleNavigation(web_contents,
                                              ui::PAGE_TRANSITION_TYPED);
    site_engagement_service->HandleUserInput(
        web_contents, SiteEngagementService::ENGAGEMENT_MOUSE);

    ExpectTotalCounts(tester, histograms, 4);
    ExpectTotalCounts(tester, ~histograms, 0);

    ExpectBucketCounts(tester, histograms,
                       SiteEngagementService::ENGAGEMENT_MEDIA_VISIBLE, 1);
    ExpectBucketCounts(tester, histograms,
                       SiteEngagementService::ENGAGEMENT_MEDIA_HIDDEN, 1);
    ExpectBucketCounts(tester, histograms,
                       SiteEngagementService::ENGAGEMENT_NAVIGATION, 1);
    ExpectBucketCounts(tester, histograms,
                       SiteEngagementService::ENGAGEMENT_MOUSE, 1);
  }

 protected:
  void CountUserInstalledApps() {
    web_app::WebAppMetrics* web_app_metrics =
        web_app::WebAppMetrics::Get(profile());
    web_app_metrics->CountUserInstalledAppsForTesting();
  }

  const extensions::Extension* InstallBookmarkAppAndCountApps(
      WebApplicationInfo info) {
    const extensions::Extension* app =
        ExtensionBrowserTest::InstallBookmarkApp(std::move(info));
    CountUserInstalledApps();
    return app;
  }

  // TODO(loyso): Merge this method with
  // PendingBookmarkAppManagerBrowserTest::InstallApp in some
  // test_support library.
  void InstallDefaultAppAndCountApps(
      web_app::ExternalInstallOptions install_options) {
    base::RunLoop run_loop;

    web_app::WebAppProvider::Get(browser()->profile())
        ->pending_app_manager()
        .Install(std::move(install_options),
                 base::BindLambdaForTesting(
                     [this, &run_loop](const GURL& provided_url,
                                       web_app::InstallResultCode code) {
                       result_code_ = code;
                       run_loop.Quit();
                     }));
    run_loop.Run();
    ASSERT_TRUE(result_code_.has_value());
    CountUserInstalledApps();
  }

  base::Optional<web_app::InstallResultCode> result_code_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkAppTest);
};

IN_PROC_BROWSER_TEST_F(BookmarkAppTest, EngagementHistogramForAppInWindow) {
  base::HistogramTester tester;

  const GURL example_url = GURL("http://example.org/");

  WebApplicationInfo web_app_info;
  web_app_info.app_url = example_url;
  web_app_info.scope = example_url;
  web_app_info.open_as_window = true;
  const extensions::Extension* app =
      InstallBookmarkAppAndCountApps(web_app_info);

  Browser* app_browser = LaunchAppBrowser(app);
  NavigateToURLAndWait(app_browser, example_url);

  EXPECT_EQ(web_app::GetAppIdFromApplicationName(app_browser->app_name()),
            app->id());

  Histograms histograms;
  histograms[kHistogramInWindow] = true;
  histograms[kHistogramUserInstalled_InWindow] = true;
  histograms[kHistogramUserInstalled_FromInstallButton_InWindow] = true;
  histograms[kHistogramUpToThreeUserInstalledApps] = true;

  TestEngagementEventWebAppLaunch(tester, histograms);
  TestEngagementEventsAfterLaunch(histograms, app_browser);
}

IN_PROC_BROWSER_TEST_F(BookmarkAppTest, EngagementHistogramForAppInTab) {
  base::HistogramTester tester;

  const GURL example_url = GURL("http://example.org/");

  WebApplicationInfo web_app_info;
  web_app_info.app_url = example_url;
  web_app_info.scope = example_url;
  web_app_info.open_as_window = false;
  const extensions::Extension* app =
      InstallBookmarkAppAndCountApps(web_app_info);

  Browser* browser = LaunchBrowserForAppInTab(app);
  EXPECT_FALSE(browser->app_controller());
  NavigateToURLAndWait(browser, example_url);

  Histograms histograms;
  histograms[kHistogramInTab] = true;
  histograms[kHistogramUserInstalled_InTab] = true;
  histograms[kHistogramUserInstalled_FromInstallButton_InTab] = true;
  histograms[kHistogramUpToThreeUserInstalledApps] = true;

  TestEngagementEventWebAppLaunch(tester, histograms);
  TestEngagementEventsAfterLaunch(histograms, browser);
}

IN_PROC_BROWSER_TEST_F(BookmarkAppTest, EngagementHistogramAppWithoutScope) {
  base::HistogramTester tester;

  const GURL example_url = GURL("http://example.org/");

  WebApplicationInfo web_app_info;
  web_app_info.app_url = example_url;
  // If app has no scope then UrlHandlers::GetUrlHandlers are empty. Therefore,
  // the app is counted as installed via the Create Shortcut button.
  web_app_info.scope = GURL();
  web_app_info.open_as_window = true;
  const extensions::Extension* app =
      InstallBookmarkAppAndCountApps(web_app_info);

  Browser* browser = LaunchAppBrowser(app);

  EXPECT_EQ(web_app::GetAppIdFromApplicationName(browser->app_name()),
            app->id());
  EXPECT_TRUE(browser->app_controller());
  NavigateToURLAndWait(browser, example_url);

  Histograms histograms;
  histograms[kHistogramInWindow] = true;
  histograms[kHistogramUserInstalled_InWindow] = true;
  histograms[kHistogramUserInstalled_FromCreateShortcutButton_InWindow] = true;
  histograms[kHistogramUpToThreeUserInstalledApps] = true;

  TestEngagementEventWebAppLaunch(tester, histograms);
  TestEngagementEventsAfterLaunch(histograms, browser);
}

IN_PROC_BROWSER_TEST_F(BookmarkAppTest, EngagementHistogramTwoApps) {
  base::HistogramTester tester;

  const GURL example_url1 = GURL("http://example.org/");
  const GURL example_url2 = GURL("http://example.com/");

  const extensions::Extension *app1, *app2;

  // Install two apps.
  {
    WebApplicationInfo web_app_info;
    web_app_info.app_url = example_url1;
    web_app_info.scope = example_url1;
    app1 = InstallBookmarkAppAndCountApps(web_app_info);
  }
  {
    WebApplicationInfo web_app_info;
    web_app_info.app_url = example_url2;
    web_app_info.scope = example_url2;
    app2 = InstallBookmarkAppAndCountApps(web_app_info);
  }

  // Launch them three times. This ensures that each launch only logs once.
  // (Since all apps receive the notification on launch, there is a danger that
  // we might log too many times.)
  Browser* app_browser1 = LaunchAppBrowser(app1);
  Browser* app_browser2 = LaunchAppBrowser(app1);
  Browser* app_browser3 = LaunchAppBrowser(app2);

  EXPECT_EQ(web_app::GetAppIdFromApplicationName(app_browser1->app_name()),
            app1->id());
  EXPECT_EQ(web_app::GetAppIdFromApplicationName(app_browser2->app_name()),
            app1->id());
  EXPECT_EQ(web_app::GetAppIdFromApplicationName(app_browser3->app_name()),
            app2->id());

  Histograms histograms;
  histograms[kHistogramInWindow] = true;
  histograms[kHistogramUserInstalled_InWindow] = true;
  histograms[kHistogramUserInstalled_FromInstallButton_InWindow] = true;
  histograms[kHistogramUpToThreeUserInstalledApps] = true;

  ExpectUniqueSamples(tester, histograms,
                      SiteEngagementService::ENGAGEMENT_WEBAPP_SHORTCUT_LAUNCH,
                      3);
  ExpectTotalCounts(tester, ~histograms, 0);
}

IN_PROC_BROWSER_TEST_F(BookmarkAppTest, EngagementHistogramManyUserApps) {
  base::HistogramTester tester;

  // More than 3 user-installed apps:
  const int num_user_apps = 4;

  std::vector<const extensions::Extension*> apps;

  // Install apps.
  const std::string base_url = "http://example";
  for (int i = 0; i < num_user_apps; ++i) {
    const GURL url = GetUrlForSuffix(base_url, i);

    WebApplicationInfo web_app_info;
    web_app_info.app_url = url;
    web_app_info.scope = url;
    const extensions::Extension* app =
        InstallBookmarkAppAndCountApps(web_app_info);
    apps.push_back(app);
  }

  // Launch apps in windows.
  for (int i = 0; i < num_user_apps; ++i) {
    Browser* browser = LaunchAppBrowser(apps[i]);

    const GURL url = GetUrlForSuffix(base_url, i);
    NavigateToURLAndWait(browser, url);
  }

  Histograms histograms;
  histograms[kHistogramInWindow] = true;
  histograms[kHistogramUserInstalled_InWindow] = true;
  histograms[kHistogramUserInstalled_FromInstallButton_InWindow] = true;
  histograms[kHistogramMoreThanThreeUserInstalledApps] = true;

  ExpectUniqueSamples(tester, histograms,
                      SiteEngagementService::ENGAGEMENT_WEBAPP_SHORTCUT_LAUNCH,
                      4);
  ExpectTotalCounts(tester, ~histograms, 0);
}

IN_PROC_BROWSER_TEST_F(BookmarkAppTest, EngagementHistogramDefaultApp) {
  base::HistogramTester tester;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL example_url(
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"));
  InstallDefaultAppAndCountApps(CreateInstallOptions(example_url));
  ASSERT_EQ(web_app::InstallResultCode::kSuccess, result_code_.value());

  const extensions::Extension* app = extensions::util::GetInstalledPwaForUrl(
      browser()->profile(), example_url);
  ASSERT_TRUE(app);
  EXPECT_TRUE(app->was_installed_by_default());

  Browser* browser = LaunchAppBrowser(app);
  NavigateToURLAndWait(browser, example_url);

  Histograms histograms;
  histograms[kHistogramInWindow] = true;
  histograms[kHistogramDefaultInstalled_InWindow] = true;
  histograms[kHistogramNoUserInstalledApps] = true;

  TestEngagementEventWebAppLaunch(tester, histograms);
  TestEngagementEventsAfterLaunch(histograms, browser);
}

IN_PROC_BROWSER_TEST_F(BookmarkAppTest,
                       EngagementHistogramNavigateAwayFromAppTab) {
  const GURL app_url = GURL("http://example.org/app/");
  const GURL outer_url = GURL("http://example.org/");

  WebApplicationInfo web_app_info;
  web_app_info.app_url = app_url;
  web_app_info.scope = app_url;
  web_app_info.open_as_window = false;
  const extensions::Extension* app =
      InstallBookmarkAppAndCountApps(web_app_info);

  Browser* browser = LaunchBrowserForAppInTab(app);
  EXPECT_FALSE(browser->app_controller());

  NavigateToURLAndWait(browser, app_url);
  {
    Histograms histograms;
    histograms[kHistogramInTab] = true;
    histograms[kHistogramUserInstalled_InTab] = true;
    histograms[kHistogramUserInstalled_FromInstallButton_InTab] = true;
    histograms[kHistogramUpToThreeUserInstalledApps] = true;
    TestEngagementEventsAfterLaunch(histograms, browser);
  }

  // Navigate away from the web app to an outer simple web site:
  NavigateToURLAndWait(browser, outer_url);
  {
    Histograms histograms;
    histograms[kHistogramUpToThreeUserInstalledApps] = true;
    TestEngagementEventsAfterLaunch(histograms, browser);
  }
}

IN_PROC_BROWSER_TEST_F(BookmarkAppTest, EngagementHistogramRecordedForNonApps) {
  base::HistogramTester tester;
  CountUserInstalledApps();

  // Launch a non-app tab in default browser.
  const GURL example_url = GURL("http://example.org/");
  NavigateToURLAndWait(browser(), example_url);

  // Check that no histograms recorded, e.g. no
  // SiteEngagementService::ENGAGEMENT_WEBAPP_SHORTCUT_LAUNCH.
  Histograms histograms;
  ExpectTotalCounts(tester, ~histograms, 0);

  // The engagement broken down by the number of apps installed must be recorded
  // for all engagement events.
  histograms[kHistogramNoUserInstalledApps] = true;
  TestEngagementEventsAfterLaunch(histograms, browser());
}
