// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/web_applications/system_web_app_integration_test.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

SystemWebAppIntegrationTest::SystemWebAppIntegrationTest()
    : SystemWebAppManagerBrowserTest(false /* install_mock */) {}

SystemWebAppIntegrationTest::~SystemWebAppIntegrationTest() = default;

Profile* SystemWebAppIntegrationTest::profile() {
  return browser()->profile();
}

void SystemWebAppIntegrationTest::ExpectSystemWebAppValid(
    web_app::SystemAppType app_type,
    const GURL& url,
    const std::string& title) {
  Browser* app_browser = WaitForSystemAppInstallAndLaunch(app_type);
  const extensions::Extension* installed_app =
      extensions::util::GetInstalledPwaForUrl(profile(), url);

  EXPECT_TRUE(GetManager().IsSystemWebApp(installed_app->id()));
  EXPECT_TRUE(installed_app->from_bookmark());

  EXPECT_EQ(title, installed_app->name());
  EXPECT_EQ(base::ASCIIToUTF16(title),
            app_browser->window()->GetNativeWindow()->GetTitle());
  EXPECT_EQ(extensions::Manifest::EXTERNAL_COMPONENT,
            installed_app->location());

  // The installed app should match the opened app window.
  EXPECT_EQ(installed_app, GetExtensionForAppBrowser(app_browser));
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  // The opened window should be showing the url with attached WebUI.
  EXPECT_EQ(url, web_contents->GetVisibleURL());

  content::TestNavigationObserver observer(web_contents);
  observer.WaitForNavigationFinished();
  EXPECT_EQ(url, web_contents->GetLastCommittedURL());

  content::WebUI* web_ui = web_contents->GetCommittedWebUI();
  ASSERT_TRUE(web_ui);
  EXPECT_TRUE(web_ui->GetController());

  // A completed navigation could change the window title. Check again.
  EXPECT_EQ(base::ASCIIToUTF16(title),
            app_browser->window()->GetNativeWindow()->GetTitle());
}
