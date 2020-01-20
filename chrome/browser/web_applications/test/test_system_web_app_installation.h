// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_SYSTEM_WEB_APP_INSTALLATION_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_SYSTEM_WEB_APP_INSTALLATION_H_

#include <memory>

#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/test/test_system_web_app_web_ui_controller_factory.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
#include "chrome/common/web_application_info.h"

namespace web_app {

// Class to setup the installation of a test System Web App.
//
// Use SetUp*() methods to create a instance of this class in test suite's
// constructor, before the profile is fully created. In tests, call
// WaitForAppInstall() to finish the installation.
class TestSystemWebAppInstallation {
 public:
  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpTabbedMultiWindowApp();
  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpStandaloneSingleWindowApp();
  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppThatReceivesLaunchDirectory();

  ~TestSystemWebAppInstallation();

  void WaitForAppInstall();

  AppId GetAppId();
  const GURL& GetAppUrl();
  SystemAppType GetType();

 private:
  TestSystemWebAppInstallation(SystemAppType type, SystemAppInfo info);

  Profile* profile_;
  std::unique_ptr<KeyedService> CreateWebAppProvider(SystemAppInfo info,
                                                     Profile* profile);
  std::unique_ptr<TestWebAppProviderCreator> test_web_app_provider_creator_;
  SystemAppType type_;
  TestSystemWebAppWebUIControllerFactory web_ui_controller_factory_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_SYSTEM_WEB_APP_INSTALLATION_H_
