// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"

using PWAConfirmationBubbleViewBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(PWAConfirmationBubbleViewBrowserTest,
                       ShowBubbleInPWAWindow) {
  auto app_info = std::make_unique<WebApplicationInfo>();
  app_info->title = base::UTF8ToUTF16("Test app");
  app_info->app_url = GURL("https://example.com");
  Profile* profile = browser()->profile();
  web_app::AppId app_id = web_app::InstallWebApp(profile, std::move(app_info));
  Browser* browser = web_app::LaunchWebAppBrowser(profile, app_id);

  app_info = std::make_unique<WebApplicationInfo>();
  app_info->title = base::UTF8ToUTF16("Test app 2");
  app_info->app_url = GURL("https://example2.com");
  app_info->open_as_window = true;
  // Tests that we don't crash when showing the install prompt in a PWA window.
  chrome::ShowPWAInstallBubble(
      browser->tab_strip_model()->GetActiveWebContents(), std::move(app_info),
      base::DoNothing());
}
