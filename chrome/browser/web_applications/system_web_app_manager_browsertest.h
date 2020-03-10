// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APP_MANAGER_BROWSERTEST_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APP_MANAGER_BROWSERTEST_H_

#include <memory>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/web_applications/test/test_system_web_app_installation.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/test/base/in_process_browser_test.h"

class Browser;
class KeyedService;

namespace apps {
struct AppLaunchParams;
}

namespace content {
class WebContents;
}

namespace web_app {

enum class SystemAppType;

// Clients should use SystemWebAppManagerBrowserTest, so test can be run with
// both the new web apps provider and the legacy bookmark apps provider.
class SystemWebAppManagerBrowserTestBase : public InProcessBrowserTest {
 public:
  // Performs common initialization for testing SystemWebAppManager features.
  // If true, |install_mock| installs a WebUIController that serves a mock
  // System PWA, and ensures the WebAppProvider associated with the startup
  // profile is a TestWebAppProviderCreator.
  explicit SystemWebAppManagerBrowserTestBase(bool install_mock = true);

  ~SystemWebAppManagerBrowserTestBase() override;

  // Returns the SystemWebAppManager for browser()->profile(). This will be a
  // TestSystemWebAppManager if initialized with |install_mock| true.
  SystemWebAppManager& GetManager();

  // Return SystemAppType of mocked app, only valid if |install_mock| is true.
  SystemAppType GetMockAppType();

  void WaitForTestSystemAppInstall();

  // Wait for system apps to install, then launch one. Waits for launched app
  // to load.
  content::WebContents* WaitForSystemAppInstallAndLoad(
      SystemAppType system_app_type);

  // Wait for system apps to install, then launch one. Returns the browser that
  // contains it.
  Browser* WaitForSystemAppInstallAndLaunch(SystemAppType system_app_type);

  // Creates a default AppLaunchParams for |system_app_type|. Launches a window.
  // Uses kSourceTest as the AppLaunchSource.
  apps::AppLaunchParams LaunchParamsForApp(SystemAppType system_app_type);

  // Invokes OpenApplication() using the test's Profile.
  content::WebContents* LaunchApp(const apps::AppLaunchParams& params);

 protected:
  std::unique_ptr<TestSystemWebAppInstallation> maybe_installation_;

 private:
  std::unique_ptr<KeyedService> CreateWebAppProvider(Profile* profile);

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SystemWebAppManagerBrowserTestBase);
};

class SystemWebAppManagerBrowserTest
    : public SystemWebAppManagerBrowserTestBase,
      public ::testing::WithParamInterface<web_app::ProviderType> {
 public:
  explicit SystemWebAppManagerBrowserTest(bool install_mock = true);
  ~SystemWebAppManagerBrowserTest() override = default;

  web_app::ProviderType provider_type() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APP_MANAGER_BROWSERTEST_H_
