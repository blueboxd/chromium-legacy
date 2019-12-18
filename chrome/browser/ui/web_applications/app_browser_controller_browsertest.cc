// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/app_browser_controller.h"

#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/test/test_system_web_app_installation.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"

namespace web_app {

class AppBrowserControllerBrowserTest : public InProcessBrowserTest {
 public:
  AppBrowserControllerBrowserTest()
      : test_system_web_app_installation_(
            TestSystemWebAppInstallation::SetUpTabbedMultiWindowApp()) {}

 protected:
  void InstallAndLaunchMockApp() {
    test_system_web_app_installation_->WaitForAppInstall();
    app_browser_ = web_app::LaunchWebAppBrowser(
        browser()->profile(), test_system_web_app_installation_->GetAppId());
    tabbed_app_url_ = test_system_web_app_installation_->GetAppUrl();
  }

  GURL GetActiveTabURL() {
    return app_browser_->tab_strip_model()
        ->GetActiveWebContents()
        ->GetVisibleURL();
  }

  Browser* app_browser_ = nullptr;
  GURL tabbed_app_url_;

 private:
  std::unique_ptr<TestSystemWebAppInstallation>
      test_system_web_app_installation_;

  DISALLOW_COPY_AND_ASSIGN(AppBrowserControllerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(AppBrowserControllerBrowserTest, TabsTest) {
  InstallAndLaunchMockApp();

  EXPECT_TRUE(app_browser_->SupportsWindowFeature(Browser::FEATURE_TABSTRIP));

  // Check URL of tab1.
  EXPECT_EQ(GetActiveTabURL(), tabbed_app_url_);
  // Create tab2 with specific URL, check URL, number of tabs.
  chrome::AddTabAt(app_browser_, GURL(chrome::kChromeUINewTabURL), -1, true);
  EXPECT_EQ(app_browser_->tab_strip_model()->count(), 2);
  EXPECT_EQ(GetActiveTabURL(), GURL("chrome://newtab/"));
  // Create tab3 with default URL, check URL, number of tabs.
  chrome::NewTab(app_browser_);
  EXPECT_EQ(app_browser_->tab_strip_model()->count(), 3);
  EXPECT_EQ(GetActiveTabURL(), tabbed_app_url_);
  // Switch to tab1, check URL.
  chrome::SelectNextTab(app_browser_);
  EXPECT_EQ(app_browser_->tab_strip_model()->count(), 3);
  EXPECT_EQ(GetActiveTabURL(), tabbed_app_url_);
  // Switch to tab2, check URL.
  chrome::SelectNextTab(app_browser_);
  EXPECT_EQ(app_browser_->tab_strip_model()->count(), 3);
  EXPECT_EQ(GetActiveTabURL(), GURL("chrome://newtab/"));
  // Switch to tab3, check URL.
  chrome::SelectNextTab(app_browser_);
  EXPECT_EQ(app_browser_->tab_strip_model()->count(), 3);
  EXPECT_EQ(GetActiveTabURL(), tabbed_app_url_);
  // Close tab3, check number of tabs.
  chrome::CloseTab(app_browser_);
  EXPECT_EQ(app_browser_->tab_strip_model()->count(), 2);
  EXPECT_EQ(GetActiveTabURL(), GURL("chrome://newtab/"));
  // Close tab2, check number of tabs.
  chrome::CloseTab(app_browser_);
  EXPECT_EQ(app_browser_->tab_strip_model()->count(), 1);
  EXPECT_EQ(GetActiveTabURL(), tabbed_app_url_);
}

}  // namespace web_app
