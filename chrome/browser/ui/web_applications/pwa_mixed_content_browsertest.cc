// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/banners/test_app_banner_manager_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/ssl_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace web_app {

class PWAMixedContentBrowserTest : public WebAppControllerBrowserTest {
 public:
  GURL GetMixedContentAppURL() {
    return https_server()->GetURL("app.com",
                                  "/ssl/page_displays_insecure_content.html");
  }
};

// Tests that mixed content is not loaded inside PWA windows.
IN_PROC_BROWSER_TEST_P(PWAMixedContentBrowserTest, MixedContentInPWA) {
  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL app_url = GetMixedContentAppURL();
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);
  CHECK(app_browser);
  web_app::CheckMixedContentFailedToLoad(app_browser);
}

// Tests that creating a shortcut app but not installing a PWA is available for
// a non-installable site.
IN_PROC_BROWSER_TEST_P(PWAMixedContentBrowserTest,
                       ShortcutMenuOptionsForNonInstallableSite) {
  auto* manager = banners::TestAppBannerManagerDesktop::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());

  ASSERT_TRUE(https_server()->Start());
  NavigateToURLAndWait(browser(), GetMixedContentAppURL());
  EXPECT_FALSE(manager->WaitForInstallableCheck());

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, browser()), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, browser()), kNotPresent);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PWAMixedContentBrowserTest,
    ::testing::Values(ControllerType::kHostedAppController,
                      ControllerType::kUnifiedControllerWithBookmarkApp,
                      ControllerType::kUnifiedControllerWithWebApp),
    ControllerTypeParamToString);

}  // namespace web_app
