// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/system_web_app_manager_browsertest.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/test/test_system_web_app_installation.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/manifest_handlers/app_theme_color_info.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace web_app {

SystemWebAppManagerBrowserTest::SystemWebAppManagerBrowserTest(
    bool install_mock) {
  scoped_feature_list_.InitWithFeatures(
      {features::kSystemWebApps, blink::features::kNativeFileSystemAPI,
       blink::features::kFileHandlingAPI},
      {});
  if (install_mock) {
    maybe_installation_ =
        TestSystemWebAppInstallation::SetUpStandaloneSingleWindowApp();
  }
}

SystemWebAppManagerBrowserTest::~SystemWebAppManagerBrowserTest() = default;

// static
const extensions::Extension*
SystemWebAppManagerBrowserTest::GetExtensionForAppBrowser(Browser* browser) {
  return static_cast<extensions::HostedAppBrowserController*>(
             browser->app_controller())
      ->GetExtensionForTesting();
}

SystemWebAppManager& SystemWebAppManagerBrowserTest::GetManager() {
  return WebAppProvider::Get(browser()->profile())->system_web_app_manager();
}

SystemAppType SystemWebAppManagerBrowserTest::GetMockAppType() {
  DCHECK(maybe_installation_);
  return maybe_installation_->GetType();
}

void SystemWebAppManagerBrowserTest::WaitForTestSystemAppInstall() {
  // Wait for the System Web Apps to install.
  if (maybe_installation_) {
    maybe_installation_->WaitForAppInstall();
  } else {
    GetManager().InstallSystemAppsForTesting();
  }
}

Browser* SystemWebAppManagerBrowserTest::WaitForSystemAppInstallAndLaunch(
    SystemAppType system_app_type) {
  WaitForTestSystemAppInstall();
  apps::AppLaunchParams params = LaunchParamsForApp(system_app_type);
  content::WebContents* web_contents = LaunchApp(params);
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  EXPECT_EQ(web_app::GetAppIdFromApplicationName(browser->app_name()),
            params.app_id);
  return browser;
}

apps::AppLaunchParams SystemWebAppManagerBrowserTest::LaunchParamsForApp(
    SystemAppType system_app_type) {
  base::Optional<AppId> app_id =
      GetManager().GetAppIdForSystemApp(system_app_type);
  DCHECK(app_id.has_value());
  return apps::AppLaunchParams(
      *app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::CURRENT_TAB,
      apps::mojom::AppLaunchSource::kSourceTest);
}

content::WebContents* SystemWebAppManagerBrowserTest::LaunchApp(
    const apps::AppLaunchParams& params) {
  // Use apps::LaunchService::OpenApplication() to get the most coverage. E.g.,
  // this is what is invoked by file_manager::file_tasks::ExecuteWebTask() on
  // ChromeOS.
  return apps::LaunchService::Get(browser()->profile())
      ->OpenApplication(params);
}

content::EvalJsResult EvalJs(content::WebContents* web_contents,
                             const std::string& script) {
  // Set world_id = 1 to bypass Content Security Policy restriction.
  return content::EvalJs(web_contents, script,
                         content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                         1 /*world_id*/);
}

::testing::AssertionResult ExecJs(content::WebContents* web_contents,
                                  const std::string& script) {
  // Set world_id = 1 to bypass Content Security Policy restriction.
  return content::ExecJs(web_contents, script,
                         content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                         1 /*world_id*/);
}

// Test that System Apps install correctly with a manifest.
IN_PROC_BROWSER_TEST_F(SystemWebAppManagerBrowserTest, Install) {
  const extensions::Extension* app = GetExtensionForAppBrowser(
      WaitForSystemAppInstallAndLaunch(GetMockAppType()));
  EXPECT_EQ("Test System App", app->name());
  EXPECT_EQ(SkColorSetRGB(0, 0xFF, 0),
            extensions::AppThemeColorInfo::GetThemeColor(app));
  EXPECT_TRUE(app->from_bookmark());
  EXPECT_EQ(extensions::Manifest::EXTERNAL_COMPONENT, app->location());

  // The app should be a PWA.
  EXPECT_EQ(extensions::util::GetInstalledPwaForUrl(
                browser()->profile(), content::GetWebUIURL("test-system-app/")),
            app);
  EXPECT_TRUE(GetManager().IsSystemWebApp(app->id()));
}

// Check the toolbar is not shown for system web apps for pages on the chrome://
// scheme but is shown off the chrome:// scheme.
IN_PROC_BROWSER_TEST_F(SystemWebAppManagerBrowserTest,
                       ToolbarVisibilityForSystemWebApp) {
  Browser* app_browser = WaitForSystemAppInstallAndLaunch(GetMockAppType());
  // In scope, the toolbar should not be visible.
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());

  // Because the first part of the url is on a different origin (settings vs.
  // foo) a toolbar would normally be shown. However, because settings is a
  // SystemWebApp and foo is served via chrome:// it is okay not to show the
  // toolbar.
  GURL out_of_scope_chrome_page(content::kChromeUIScheme +
                                std::string("://foo"));
  content::NavigateToURLBlockUntilNavigationsComplete(
      app_browser->tab_strip_model()->GetActiveWebContents(),
      out_of_scope_chrome_page, 1);
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());

  // Even though the url is secure it is not being served over chrome:// so a
  // toolbar should be shown.
  GURL off_scheme_page("https://example.com");
  content::NavigateToURLBlockUntilNavigationsComplete(
      app_browser->tab_strip_model()->GetActiveWebContents(), off_scheme_page,
      1);
  EXPECT_TRUE(app_browser->app_controller()->ShouldShowCustomTabBar());
}

// Check launch files are passed to application.
IN_PROC_BROWSER_TEST_F(SystemWebAppManagerBrowserTest,
                       LaunchFilesForSystemWebApp) {
  WaitForTestSystemAppInstall();
  apps::AppLaunchParams params = LaunchParamsForApp(GetMockAppType());
  params.source = apps::mojom::AppLaunchSource::kSourceChromeInternal;

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  base::FilePath temp_file_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_directory.GetPath(),
                                             &temp_file_path));

  const GURL& launch_url = WebAppProvider::Get(browser()->profile())
                               ->registrar()
                               .GetAppLaunchURL(params.app_id);

  // First launch.
  params.launch_files = {temp_file_path};
  content::TestNavigationObserver navigation_observer(launch_url);
  navigation_observer.StartWatchingNewWebContents();
  content::WebContents* web_contents =
      OpenApplication(browser()->profile(), params);
  navigation_observer.Wait();

  // Set up a Promise that resolves to launchParams, when launchQueue's consumer
  // callback is called.
  EXPECT_TRUE(ExecJs(web_contents,
                     "window.launchParamsPromise = new Promise(resolve => {"
                     "  window.resolveLaunchParamsPromise = resolve;"
                     "});"
                     "launchQueue.setConsumer(launchParams => {"
                     "  window.resolveLaunchParamsPromise(launchParams);"
                     "});"));

  // Check launch files are correct.
  EXPECT_EQ(temp_file_path.BaseName().AsUTF8Unsafe(),
            EvalJs(web_contents,
                   "window.launchParamsPromise.then("
                   "  launchParams => launchParams.files[0].name);"));

  // Reset the Promise to get second launchParams.
  EXPECT_TRUE(ExecJs(web_contents,
                     "window.launchParamsPromise = new Promise(resolve => {"
                     "  window.resolveLaunchParamsPromise = resolve;"
                     "});"));

  // Second Launch.
  base::FilePath temp_file_path2;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_directory.GetPath(),
                                             &temp_file_path2));
  params.launch_files = {temp_file_path2};
  content::WebContents* web_contents2 =
      OpenApplication(browser()->profile(), params);

  // WebContents* should be the same because we are passing launchParams to the
  // opened application.
  EXPECT_EQ(web_contents, web_contents2);

  // Second launch_files are passed to the opened application.
  EXPECT_EQ(temp_file_path2.BaseName().AsUTF8Unsafe(),
            EvalJs(web_contents,
                   "window.launchParamsPromise.then("
                   "  launchParams => launchParams.files[0].name)"));
}

class SystemWebAppManagerLaunchFilesBrowserTest
    : public SystemWebAppManagerBrowserTest {
 public:
  SystemWebAppManagerLaunchFilesBrowserTest()
      : SystemWebAppManagerBrowserTest(/*install_mock=*/false) {
    maybe_installation_ =
        TestSystemWebAppInstallation::SetUpAppThatReceivesLaunchDirectory();
  }
};

// Launching behavior for apps that do not want to received launch directory are
// tested in |SystemWebAppManagerBrowserTest.LaunchFilesForSystemWebApp|.
IN_PROC_BROWSER_TEST_F(SystemWebAppManagerLaunchFilesBrowserTest,
                       LaunchDirectoryForSystemWebApp) {
  WaitForTestSystemAppInstall();
  apps::AppLaunchParams params = LaunchParamsForApp(GetMockAppType());
  params.source = apps::mojom::AppLaunchSource::kSourceChromeInternal;

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  base::FilePath temp_file_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_directory.GetPath(),
                                             &temp_file_path));

  const GURL& launch_url = WebAppProvider::Get(browser()->profile())
                               ->registrar()
                               .GetAppLaunchURL(params.app_id);

  // First launch.
  params.launch_files = {temp_file_path};
  content::TestNavigationObserver navigation_observer(launch_url);
  navigation_observer.StartWatchingNewWebContents();
  content::WebContents* web_contents =
      OpenApplication(browser()->profile(), params);
  navigation_observer.Wait();

  // Set up a Promise that resolves to launchParams, when launchQueue's consumer
  // callback is called.
  EXPECT_TRUE(ExecJs(web_contents,
                     "window.launchParamsPromise = new Promise(resolve => {"
                     "  window.resolveLaunchParamsPromise = resolve;"
                     "});"
                     "launchQueue.setConsumer(launchParams => {"
                     "  window.resolveLaunchParamsPromise(launchParams);"
                     "});"));

  // Wait for launch. Set window.firstLaunchParams for inspection.
  EXPECT_TRUE(ExecJs(web_contents,
                     "window.launchParamsPromise.then(launchParams => {"
                     "  window.firstLaunchParams = launchParams;"
                     "});"));

  // Check launch directory is correct.
  EXPECT_EQ(true, EvalJs(web_contents,
                         "window.firstLaunchParams.files[0].isDirectory"));
  EXPECT_EQ(temp_directory.GetPath().BaseName().AsUTF8Unsafe(),
            EvalJs(web_contents, "window.firstLaunchParams.files[0].name"));

  // Check launch files are correct.
  EXPECT_EQ(true,
            EvalJs(web_contents, "window.firstLaunchParams.files[1].isFile"));
  EXPECT_EQ(temp_file_path.BaseName().AsUTF8Unsafe(),
            EvalJs(web_contents, "window.firstLaunchParams.files[1].name"));

  // Reset the Promise to get second launchParams.
  EXPECT_TRUE(ExecJs(web_contents,
                     "window.launchParamsPromise = new Promise(resolve => {"
                     "  window.resolveLaunchParamsPromise = resolve;"
                     "});"));

  // Second Launch.
  base::ScopedTempDir temp_directory2;
  ASSERT_TRUE(temp_directory2.CreateUniqueTempDir());
  base::FilePath temp_file_path2;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_directory2.GetPath(),
                                             &temp_file_path2));
  params.launch_files = {temp_file_path2};
  content::WebContents* web_contents2 =
      OpenApplication(browser()->profile(), params);

  // WebContents* should be the same because we are passing launchParams to the
  // opened application.
  EXPECT_EQ(web_contents, web_contents2);

  // Wait for launch. Sets window.secondLaunchParams for inspection.
  EXPECT_TRUE(ExecJs(web_contents,
                     "window.launchParamsPromise.then(launchParams => {"
                     "  window.secondLaunchParams = launchParams;"
                     "});"));

  // Second launch_dir and launch_files are passed to the opened application.
  EXPECT_EQ(true, EvalJs(web_contents,
                         "window.secondLaunchParams.files[0].isDirectory"));
  EXPECT_EQ(temp_directory2.GetPath().BaseName().AsUTF8Unsafe(),
            EvalJs(web_contents, "window.secondLaunchParams.files[0].name"));
  EXPECT_EQ(true,
            EvalJs(web_contents, "window.secondLaunchParams.files[1].isFile"));
  EXPECT_EQ(temp_file_path2.BaseName().AsUTF8Unsafe(),
            EvalJs(web_contents, "window.secondLaunchParams.files[1].name"));
}

}  // namespace web_app
