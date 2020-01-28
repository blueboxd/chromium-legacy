// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_WEB_APP_BROWSERTEST_UTIL_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_WEB_APP_BROWSERTEST_UTIL_H_

#include <memory>

#include "chrome/browser/web_applications/components/web_app_id.h"
#include "url/gurl.h"

class Browser;
class Profile;
struct WebApplicationInfo;

namespace web_app {

struct ExternalInstallOptions;
enum class InstallResultCode;

// Synchronous version of InstallManager::InstallWebAppFromInfo.
AppId InstallWebApp(Profile* profile, std::unique_ptr<WebApplicationInfo>);

// Launches a new app window for |app| in |profile|.
Browser* LaunchWebAppBrowser(Profile*, const AppId&);

// Launches a new tab for |app| in |profile|.
Browser* LaunchBrowserForWebAppInTab(Profile*, const AppId&);

// Return |ExternalInstallOptions| with OS shortcut creation disabled.
ExternalInstallOptions CreateInstallOptions(const GURL& url);

// Synchronous version of PendingAppManager::Install.
InstallResultCode PendingAppManagerInstall(Profile*, ExternalInstallOptions);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_WEB_APP_BROWSERTEST_UTIL_H_
