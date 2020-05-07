// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/web_applications/system_web_app_integration_test.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/system_web_app_manager_browsertest.h"
#include "chromeos/components/help_app_ui/url_constants.h"
#include "chromeos/components/web_applications/test/sandboxed_web_ui_test_base.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/display_constants.h"

class HelpAppIntegrationTest : public SystemWebAppIntegrationTest {
 public:
  HelpAppIntegrationTest() {
    scoped_feature_list_.InitWithFeatures({chromeos::features::kHelpAppV2}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that the Help App installs and launches correctly. Runs some spot
// checks on the manifest.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2) {
  const GURL url(chromeos::kChromeUIHelpAppURL);
  EXPECT_NO_FATAL_FAILURE(
      ExpectSystemWebAppValid(web_app::SystemAppType::HELP, url, "Discover"));
}

// Test that the Help App is searchable by additional strings.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2SearchInLauncher) {
  WaitForSystemAppInstallAndLaunch(web_app::SystemAppType::HELP);
  EXPECT_EQ(
      std::vector<std::string>({"Get Help", "Perks", "Offers"}),
      GetManager().GetAdditionalSearchTerms(web_app::SystemAppType::HELP));
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
  content::WebContents* web_contents =
      WaitForSystemAppInstallAndLoad(web_app::SystemAppType::HELP);
  base::UserActionTester user_action_tester;

  constexpr char kScript[] = R"(
    chrome.metricsPrivate.recordUserAction("Discover.Help.TabClicked");
  )";

  EXPECT_EQ(0, user_action_tester.GetActionCount("Discover.Help.TabClicked"));
  EXPECT_EQ(nullptr,
            SandboxedWebUiAppTestBase::EvalJsInAppFrame(web_contents, kScript));
  EXPECT_EQ(1, user_action_tester.GetActionCount("Discover.Help.TabClicked"));
}

INSTANTIATE_TEST_SUITE_P(All,
                         HelpAppIntegrationTest,
                         ::testing::Values(web_app::ProviderType::kBookmarkApps,
                                           web_app::ProviderType::kWebApps),
                         web_app::ProviderTypeParamToString);
