// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/user_action_tester.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/banners/test_app_banner_manager_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/test/web_app_install_observer.h"
#include "url/gurl.h"

namespace web_app {

class CreateShortcutBrowserTest : public WebAppControllerBrowserTest {
 public:
  AppId InstallShortcutAppForCurrentUrl() {
    chrome::SetAutoAcceptBookmarkAppDialogForTesting(true, false);
    WebAppInstallObserver observer(profile());
    CHECK(chrome::ExecuteCommand(browser(), IDC_CREATE_SHORTCUT));
    AppId app_id = observer.AwaitNextInstall();
    chrome::SetAutoAcceptBookmarkAppDialogForTesting(false, false);
    return app_id;
  }

  AppRegistrar& registrar() {
    auto* provider = WebAppProviderBase::GetProviderBase(profile());
    CHECK(provider);
    return provider->registrar();
  }

  AppRegistryController& registry_controller() {
    auto* provider = WebAppProviderBase::GetProviderBase(profile());
    CHECK(provider);
    return provider->registry_controller();
  }

};

IN_PROC_BROWSER_TEST_P(CreateShortcutBrowserTest,
                       CreateShortcutForInstallableSite) {
  base::UserActionTester user_action_tester;
  ASSERT_TRUE(https_server()->Start());
  NavigateToURLAndWait(browser(), GetInstallableAppURL());

  AppId app_id = InstallShortcutAppForCurrentUrl();
  EXPECT_EQ(registrar().GetAppShortName(app_id), GetInstallableAppName());
  // Bookmark apps to PWAs should launch in a tab.
  EXPECT_EQ(registrar().GetAppUserDisplayMode(app_id), DisplayMode::kBrowser);

  EXPECT_EQ(0, user_action_tester.GetActionCount("InstallWebAppFromMenu"));
  EXPECT_EQ(1, user_action_tester.GetActionCount("CreateShortcut"));
}

IN_PROC_BROWSER_TEST_P(CreateShortcutBrowserTest,
                       CanInstallOverTabShortcutApp) {
  ASSERT_TRUE(https_server()->Start());

  NavigateToURLAndWait(browser(), GetInstallableAppURL());
  InstallShortcutAppForCurrentUrl();

  Browser* new_browser =
      NavigateInNewWindowAndAwaitInstallabilityCheck(GetInstallableAppURL());

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, new_browser),
            kNotPresent);
}

IN_PROC_BROWSER_TEST_P(CreateShortcutBrowserTest,
                       CannotInstallOverWindowShortcutApp) {
  ASSERT_TRUE(https_server()->Start());

  NavigateToURLAndWait(browser(), GetInstallableAppURL());
  AppId app_id = InstallShortcutAppForCurrentUrl();
  // Change launch container to open in window.
  registry_controller().SetAppUserDisplayMode(app_id, DisplayMode::kStandalone);

  Browser* new_browser =
      NavigateInNewWindowAndAwaitInstallabilityCheck(GetInstallableAppURL());

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, new_browser), kNotPresent);
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, new_browser),
            kEnabled);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CreateShortcutBrowserTest,
    ::testing::Values(ControllerType::kHostedAppController,
                      ControllerType::kUnifiedControllerWithBookmarkApp,
                      ControllerType::kUnifiedControllerWithWebApp),
    ControllerTypeParamToString);

}  // namespace web_app
