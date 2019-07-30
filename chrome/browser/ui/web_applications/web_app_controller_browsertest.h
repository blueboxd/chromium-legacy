// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_CONTROLLER_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_CONTROLLER_BROWSERTEST_H_

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/web_application_info.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

class Browser;

namespace web_app {

// TODO(https://crbug.com/966290): Add kUnifiedControllerWithWebApp.
enum class ControllerType {
  kHostedAppController,
  kUnifiedControllerWithBookmarkApp,
};

// Base class for tests of user interface support for web applications.
// ControllerType selects between use of WebAppBrowserController and
// HostedAppBrowserController.
class WebAppControllerBrowserTest
    : public extensions::ExtensionBrowserTest,
      public ::testing::WithParamInterface<ControllerType> {
 public:
  WebAppControllerBrowserTest();
  ~WebAppControllerBrowserTest() = 0;

  // ExtensionBrowserTest:
  void SetUp() override;

 protected:
  AppId InstallPWA(const GURL& app_url);

  AppId InstallWebApp(std::unique_ptr<WebApplicationInfo>&& web_app_info);

  Browser* LaunchAppBrowser(const AppId&);

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_;

  DISALLOW_COPY_AND_ASSIGN(WebAppControllerBrowserTest);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_CONTROLLER_BROWSERTEST_H_
