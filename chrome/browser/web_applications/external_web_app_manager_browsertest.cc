// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/external_web_app_manager.h"

#include "base/strings/string_util.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kChromeAppDirectory[] = "app";
const char kChromeAppName[] = "App Test";

}  // namespace

namespace web_app {

class ExternalWebAppManagerBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  ExternalWebAppManagerBrowserTest() {
    ExternalWebAppManager::SkipStartupScanForTesting();
  }

  GURL GetAppUrl() const {
    return embedded_test_server()->GetURL("/web_apps/basic.html");
  }

  ~ExternalWebAppManagerBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(ExternalWebAppManagerBrowserTest, UninstallAndReplace) {
  ASSERT_TRUE(embedded_test_server()->Start());
  Profile* profile = browser()->profile();

  // Install Chrome app to be replaced.
  const extensions::Extension* app = InstallExtensionWithSourceAndFlags(
      test_data_dir_.AppendASCII(kChromeAppDirectory), 1,
      extensions::Manifest::INTERNAL, extensions::Extension::NO_FLAGS);
  EXPECT_EQ(app->name(), kChromeAppName);

  // Start listening for Chrome app uninstall.
  extensions::TestExtensionRegistryObserver uninstall_observer(
      extensions::ExtensionRegistry::Get(profile));

  // Trigger default web app install.
  base::RunLoop sync_run_loop;
  WebAppProvider::Get(profile)
      ->external_web_app_manager_for_testing()
      .SynchronizeAppsForTesting(
          {base::ReplaceStringPlaceholders(
              R"({
                "app_url": "$1",
                "launch_container": "window",
                "user_type": ["unmanaged"],
                "uninstall_and_replace": ["$2"]
              })",
              {GetAppUrl().spec(), app->id()}, nullptr)},
          base::BindLambdaForTesting(
              [&](std::map<GURL, InstallResultCode> install_results,
                  std::map<GURL, bool> uninstall_results) {
                EXPECT_EQ(install_results.at(GetAppUrl()),
                          InstallResultCode::kSuccessNewInstall);
                sync_run_loop.Quit();
              }));
  sync_run_loop.Run();

  // Chrome app should get uninstalled.
  scoped_refptr<const extensions::Extension> uninstalled_app =
      uninstall_observer.WaitForExtensionUninstalled();
  EXPECT_EQ(app, uninstalled_app.get());
}

}  // namespace web_app
