// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_SYSTEM_WEB_APP_INSTALLATION_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_SYSTEM_WEB_APP_INSTALLATION_H_

#include <memory>
#include <string>

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
  enum IncludeLaunchDirectory { kYes, kNo };

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpTabbedMultiWindowApp();
  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpStandaloneSingleWindowApp();
  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppThatReceivesLaunchFiles(
      IncludeLaunchDirectory include_launch_directory);
  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppWithEnabledOriginTrials(const OriginTrialsMap& origin_to_trials);
  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppNotShownInLauncher();
  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppNotShownInSearch();
  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppWithAdditionalSearchTerms();
  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpChromeUntrustedApp();

  ~TestSystemWebAppInstallation();

  void WaitForAppInstall();

  AppId GetAppId();
  const GURL& GetAppUrl();
  SystemAppType GetType();

  // Override the contents served by chrome://test-system-app/manifest.json.
  void SetManifest(std::string manifest);

  void set_update_policy(SystemWebAppManager::UpdatePolicy update_policy) {
    update_policy_ = update_policy;
  }

 private:
  TestSystemWebAppInstallation(SystemAppType type, SystemAppInfo info);

  Profile* profile_;
  std::unique_ptr<KeyedService> CreateWebAppProvider(SystemAppInfo info,
                                                     Profile* profile);

  SystemWebAppManager::UpdatePolicy update_policy_ =
      SystemWebAppManager::UpdatePolicy::kAlwaysUpdate;
  std::unique_ptr<TestWebAppProviderCreator> test_web_app_provider_creator_;
  const SystemAppType type_;
  std::unique_ptr<TestSystemWebAppWebUIControllerFactory>
      web_ui_controller_factory_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_SYSTEM_WEB_APP_INSTALLATION_H_
