// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/system_web_app_manager_browsertest.h"

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/native_file_system/native_file_system_permission_request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/test/test_system_web_app_installation.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/services/app_service/public/cpp/app_registry_cache.h"
#include "chrome/services/app_service/public/cpp/app_update.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/extension_app_item.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#endif

namespace web_app {

SystemWebAppManagerBrowserTestBase::SystemWebAppManagerBrowserTestBase(
    bool install_mock) {
  scoped_feature_list_.InitWithFeatures({features::kSystemWebApps}, {});
  if (install_mock) {
    maybe_installation_ =
        TestSystemWebAppInstallation::SetUpStandaloneSingleWindowApp();
  }
}

SystemWebAppManagerBrowserTestBase::~SystemWebAppManagerBrowserTestBase() =
    default;

SystemWebAppManager& SystemWebAppManagerBrowserTestBase::GetManager() {
  return WebAppProvider::Get(browser()->profile())->system_web_app_manager();
}

SystemAppType SystemWebAppManagerBrowserTestBase::GetMockAppType() {
  DCHECK(maybe_installation_);
  return maybe_installation_->GetType();
}

void SystemWebAppManagerBrowserTestBase::WaitForTestSystemAppInstall() {
  // Wait for the System Web Apps to install.
  if (maybe_installation_) {
    maybe_installation_->WaitForAppInstall();
  } else {
    GetManager().InstallSystemAppsForTesting();
  }
  // Ensure apps are registered with the |AppService| and populated in
  // |AppListModel|.
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
  proxy->FlushMojoCallsForTesting();
}

content::WebContents*
SystemWebAppManagerBrowserTestBase::WaitForSystemAppInstallAndLoad(
    SystemAppType system_app_type) {
  WaitForTestSystemAppInstall();
  apps::AppLaunchParams params = LaunchParamsForApp(system_app_type);
  content::WebContents* web_contents = LaunchApp(params);
  EXPECT_TRUE(WaitForLoadStop(web_contents));
  return web_contents;
}

Browser* SystemWebAppManagerBrowserTestBase::WaitForSystemAppInstallAndLaunch(
    SystemAppType system_app_type) {
  WaitForTestSystemAppInstall();
  apps::AppLaunchParams params = LaunchParamsForApp(system_app_type);
  content::WebContents* web_contents = LaunchApp(params);
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  EXPECT_EQ(web_app::GetAppIdFromApplicationName(browser->app_name()),
            params.app_id);
  return browser;
}

apps::AppLaunchParams SystemWebAppManagerBrowserTestBase::LaunchParamsForApp(
    SystemAppType system_app_type) {
  base::Optional<AppId> app_id =
      GetManager().GetAppIdForSystemApp(system_app_type);
  DCHECK(app_id.has_value());
  return apps::AppLaunchParams(
      *app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::CURRENT_TAB,
      apps::mojom::AppLaunchSource::kSourceTest);
}

content::WebContents* SystemWebAppManagerBrowserTestBase::LaunchApp(
    const apps::AppLaunchParams& params) {
  // Use apps::LaunchService::OpenApplication() to get the most coverage. E.g.,
  // this is what is invoked by file_manager::file_tasks::ExecuteWebTask() on
  // ChromeOS.
  return apps::LaunchService::Get(browser()->profile())
      ->OpenApplication(params);
}

SystemWebAppManagerBrowserTest::SystemWebAppManagerBrowserTest(
    bool install_mock)
    : SystemWebAppManagerBrowserTestBase(install_mock) {
  if (GetParam() == ProviderType::kWebApps) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kDesktopPWAsWithoutExtensions);
  } else if (GetParam() == ProviderType::kBookmarkApps) {
    scoped_feature_list_.InitAndDisableFeature(
        features::kDesktopPWAsWithoutExtensions);
  }
}

// Test that System Apps install correctly with a manifest.
IN_PROC_BROWSER_TEST_P(SystemWebAppManagerBrowserTest, Install) {
  Browser* app_browser = WaitForSystemAppInstallAndLaunch(GetMockAppType());

  AppId app_id = app_browser->app_controller()->GetAppId();
  EXPECT_EQ(GetManager().GetAppIdForSystemApp(GetMockAppType()), app_id);
  EXPECT_TRUE(GetManager().IsSystemWebApp(app_id));

  Profile* profile = app_browser->profile();
  AppRegistrar& registrar =
      WebAppProviderBase::GetProviderBase(profile)->registrar();

  EXPECT_EQ("Test System App", registrar.GetAppShortName(app_id));
  EXPECT_EQ(SkColorSetRGB(0, 0xFF, 0), registrar.GetAppThemeColor(app_id));
  EXPECT_TRUE(registrar.HasExternalAppWithInstallSource(
      app_id, web_app::ExternalInstallSource::kSystemInstalled));
  EXPECT_EQ(
      registrar.FindAppWithUrlInScope(content::GetWebUIURL("test-system-app/")),
      app_id);

  if (!base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions)) {
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
            app_id);
    EXPECT_TRUE(extension->from_bookmark());
    EXPECT_EQ(extensions::Manifest::EXTERNAL_COMPONENT, extension->location());
  }

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
  proxy->AppRegistryCache().ForOneApp(
      app_id, [](const apps::AppUpdate& update) {
        EXPECT_EQ(apps::mojom::OptionalBool::kTrue, update.ShowInLauncher());
      });
}

// Check the toolbar is not shown for system web apps for pages on the chrome://
// scheme but is shown off the chrome:// scheme.
IN_PROC_BROWSER_TEST_P(SystemWebAppManagerBrowserTest,
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

class SystemWebAppManagerFileHandlingBrowserTestBase
    : public SystemWebAppManagerBrowserTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  using IncludeLaunchDirectory =
      TestSystemWebAppInstallation::IncludeLaunchDirectory;
  explicit SystemWebAppManagerFileHandlingBrowserTestBase(
      IncludeLaunchDirectory include_launch_directory)
      : SystemWebAppManagerBrowserTestBase(/*install_mock=*/false) {
    bool enable_origin_scoped_permission_context;
    bool enable_desktop_pwas_without_extensions;
    std::tie(enable_origin_scoped_permission_context,
             enable_desktop_pwas_without_extensions) = GetParam();

    scoped_feature_permission_context_.InitWithFeatureState(
        features::kNativeFileSystemOriginScopedPermissions,
        enable_origin_scoped_permission_context);
    scoped_feature_web_app_provider_type_.InitWithFeatureState(
        features::kDesktopPWAsWithoutExtensions,
        enable_desktop_pwas_without_extensions);
    scoped_feature_blink_api_.InitWithFeatures(
        {blink::features::kNativeFileSystemAPI,
         blink::features::kFileHandlingAPI},
        {});

    maybe_installation_ =
        TestSystemWebAppInstallation::SetUpAppThatReceivesLaunchFiles(
            include_launch_directory);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_permission_context_;
  base::test::ScopedFeatureList scoped_feature_web_app_provider_type_;
  base::test::ScopedFeatureList scoped_feature_blink_api_;
};

class SystemWebAppManagerLaunchFilesBrowserTest
    : public SystemWebAppManagerFileHandlingBrowserTestBase {
 public:
  SystemWebAppManagerLaunchFilesBrowserTest()
      : SystemWebAppManagerFileHandlingBrowserTestBase(
            IncludeLaunchDirectory::kNo) {}
};

// Check launch files are passed to application.
// Note: This test uses ExecuteScriptXXX instead of ExecJs and EvalJs because of
// some quirks surrounding origin trials and content security policies.
IN_PROC_BROWSER_TEST_P(SystemWebAppManagerLaunchFilesBrowserTest,
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
      apps::LaunchService::Get(browser()->profile())->OpenApplication(params);
  navigation_observer.Wait();

  // Set up a Promise that resolves to launchParams, when launchQueue's consumer
  // callback is called.
  EXPECT_TRUE(content::ExecuteScript(
      web_contents,
      "window.launchParamsPromise = new Promise(resolve => {"
      "  window.resolveLaunchParamsPromise = resolve;"
      "});"
      "launchQueue.setConsumer(launchParams => {"
      "  window.resolveLaunchParamsPromise(launchParams);"
      "});"));

  // Check launch files are correct.
  std::string file_name;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "window.launchParamsPromise.then("
      "  launchParams => "
      "    domAutomationController.send(launchParams.files[0].name));",
      &file_name));
  EXPECT_EQ(temp_file_path.BaseName().AsUTF8Unsafe(), file_name);

  // Reset the Promise to get second launchParams.
  EXPECT_TRUE(content::ExecuteScript(
      web_contents,
      "window.launchParamsPromise = new Promise(resolve => {"
      "  window.resolveLaunchParamsPromise = resolve;"
      "});"));

  // Second Launch.
  base::FilePath temp_file_path2;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_directory.GetPath(),
                                             &temp_file_path2));
  params.launch_files = {temp_file_path2};
  content::WebContents* web_contents2 =
      apps::LaunchService::Get(browser()->profile())->OpenApplication(params);

  // WebContents* should be the same because we are passing launchParams to the
  // opened application.
  EXPECT_EQ(web_contents, web_contents2);

  // Second launch_files are passed to the opened application.
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "window.launchParamsPromise.then("
      "  launchParams => "
      "    domAutomationController.send(launchParams.files[0].name))",
      &file_name));
  EXPECT_EQ(temp_file_path2.BaseName().AsUTF8Unsafe(), file_name);
}

class SystemWebAppManagerLaunchDirectoryBrowserTest
    : public SystemWebAppManagerFileHandlingBrowserTestBase {
 public:
  SystemWebAppManagerLaunchDirectoryBrowserTest()
      : SystemWebAppManagerFileHandlingBrowserTestBase(
            IncludeLaunchDirectory::kYes) {}
};

// Launching behavior for apps that do not want to received launch directory are
// tested in |SystemWebAppManagerBrowserTestBase.LaunchFilesForSystemWebApp|.
// Note: This test uses ExecuteScriptXXX instead of ExecJs and EvalJs because of
// some quirks surrounding origin trials and content security policies.
IN_PROC_BROWSER_TEST_P(SystemWebAppManagerLaunchDirectoryBrowserTest,
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
      apps::LaunchService::Get(browser()->profile())->OpenApplication(params);
  navigation_observer.Wait();

  // Set up a Promise that resolves to launchParams, when launchQueue's consumer
  // callback is called.
  EXPECT_TRUE(content::ExecuteScript(
      web_contents,
      "window.launchParamsPromise = new Promise(resolve => {"
      "  window.resolveLaunchParamsPromise = resolve;"
      "});"
      "launchQueue.setConsumer(launchParams => {"
      "  window.resolveLaunchParamsPromise(launchParams);"
      "});"));

  // Wait for launch. Set window.firstLaunchParams for inspection.
  EXPECT_TRUE(
      content::ExecuteScript(web_contents,
                             "window.launchParamsPromise.then(launchParams => {"
                             "  window.firstLaunchParams = launchParams;"
                             "});"));

  // Check launch directory is correct.
  bool is_directory;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "domAutomationController.send(window.firstLaunchParams."
      "files[0].isDirectory)",
      &is_directory));
  EXPECT_TRUE(is_directory);

  std::string file_name;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "domAutomationController.send(window.firstLaunchParams.files[0].name)",
      &file_name));
  EXPECT_EQ(temp_directory.GetPath().BaseName().AsUTF8Unsafe(), file_name);

  // Check launch files are correct.
  bool is_file;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "domAutomationController.send(window.firstLaunchParams.files[1].isFile)",
      &is_file));
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "domAutomationController.send(window.firstLaunchParams.files[1].name)",
      &file_name));
  EXPECT_EQ(temp_file_path.BaseName().AsUTF8Unsafe(), file_name);

  // Reset the Promise to get second launchParams.
  EXPECT_TRUE(content::ExecuteScript(
      web_contents,
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
      apps::LaunchService::Get(browser()->profile())->OpenApplication(params);

  // WebContents* should be the same because we are passing launchParams to the
  // opened application.
  EXPECT_EQ(web_contents, web_contents2);

  // Wait for launch. Sets window.secondLaunchParams for inspection.
  EXPECT_TRUE(
      content::ExecuteScript(web_contents,
                             "window.launchParamsPromise.then(launchParams => {"
                             "  window.secondLaunchParams = launchParams;"
                             "});"));

  // Second launch_dir and launch_files are passed to the opened application.
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "domAutomationController.send(window.secondLaunchParams.files[0]."
      "isDirectory)",
      &is_directory));
  EXPECT_TRUE(is_directory);

  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "domAutomationController.send(window.secondLaunchParams.files[0].name)",
      &file_name));
  EXPECT_EQ(temp_directory2.GetPath().BaseName().AsUTF8Unsafe(), file_name);

  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "domAutomationController.send(window.secondLaunchParams.files[1]."
      "isFile)",
      &is_file));
  EXPECT_TRUE(is_file);

  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "domAutomationController.send(window.secondLaunchParams.files[1].name)",
      &file_name));
  EXPECT_EQ(temp_file_path2.BaseName().AsUTF8Unsafe(), file_name);

  // Launch directories and files passed to system web apps should automatically
  // be granted write permission. Users should not get permission prompts. Here
  // we execute some JavaScript code that modifies and deletes files in the
  // directory.

  // Auto deny prompts (if they show up).
  NativeFileSystemPermissionRequestManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(permissions::PermissionAction::DENIED);

  // Modifies the launch file. Reuse the first launch directory.
  bool writer_closed;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.firstLaunchParams.files[1].createWritable().then("
      "  async writer => {"
      "    console.log(writer);"
      "    await writer.write('test');"
      "    await writer.close();"
      "    domAutomationController.send(true);"
      "  }"
      ");",
      &writer_closed));
  EXPECT_TRUE(writer_closed);

  std::string read_contents;
  EXPECT_TRUE(base::ReadFileToString(temp_file_path, &read_contents));
  EXPECT_EQ("test", read_contents);

  // Deletes the launch file. Reuse the second launch directory.
  bool file_removed;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.secondLaunchParams.files[0].removeEntry("
      "  window.secondLaunchParams.files[1].name"
      ").then(_ => domAutomationController.send(true));",
      &file_removed));
  EXPECT_TRUE(file_removed);
  EXPECT_FALSE(base::PathExists(temp_file_path2));
}

class SystemWebAppManagerFileHandlingOriginTrialsBrowserTest
    : public SystemWebAppManagerBrowserTest {
 public:
  SystemWebAppManagerFileHandlingOriginTrialsBrowserTest()
      : SystemWebAppManagerBrowserTest(/*install_mock=*/false) {
    maybe_installation_ =
        TestSystemWebAppInstallation::SetUpAppWithEnabledOriginTrials(
            OriginTrialsMap({{GetOrigin(GURL("chrome://test-system-app/")),
                              {"NativeFileSystem2", "FileHandling"}}}));
  }

  ~SystemWebAppManagerFileHandlingOriginTrialsBrowserTest() override = default;

 private:
  url::Origin GetOrigin(const GURL& url) { return url::Origin::Create(url); }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerFileHandlingOriginTrialsBrowserTest,
                       FileHandlingWorks) {
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

  params.launch_files = {temp_file_path};
  content::TestNavigationObserver navigation_observer(launch_url);
  navigation_observer.StartWatchingNewWebContents();
  content::WebContents* web_contents =
      apps::LaunchService::Get(browser()->profile())->OpenApplication(params);
  navigation_observer.Wait();

  // Wait for the Promise to resolve.
  bool promise_resolved = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "launchQueue.setConsumer(launchParams => {"
      "  domAutomationController.send(true);"
      "});",
      &promise_resolved));
  EXPECT_TRUE(promise_resolved);
}

class SystemWebAppManagerNotShownInLauncherTest
    : public SystemWebAppManagerBrowserTest {
 public:
  SystemWebAppManagerNotShownInLauncherTest()
      : SystemWebAppManagerBrowserTest(/*install_mock=*/false) {
    maybe_installation_ =
        TestSystemWebAppInstallation::SetUpAppNotShownInLauncher();
  }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerNotShownInLauncherTest,
                       NotShownInLauncher) {
  // TODO(crbug.com/1054195): Make the expectation unconditional.
  const web_app::ProviderType provider = provider_type();

  WaitForSystemAppInstallAndLaunch(GetMockAppType());
  AppId app_id = GetManager().GetAppIdForSystemApp(GetMockAppType()).value();

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
  proxy->AppRegistryCache().ForOneApp(
      app_id, [provider](const apps::AppUpdate& update) {
        if (provider == ProviderType::kWebApps) {
          EXPECT_EQ(apps::mojom::OptionalBool::kTrue, update.ShowInLauncher());
        } else {
          EXPECT_EQ(apps::mojom::OptionalBool::kFalse, update.ShowInLauncher());
        }
      });
  // OS Integration only relevant for Chrome OS.
#if defined(OS_CHROMEOS)
  // The |AppList| should have all apps visible in the launcher, apps get
  // removed from the |AppList| when they are hidden.
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  ASSERT_TRUE(client);
  AppListModelUpdater* model_updater = test::GetModelUpdater(client);
  const ChromeAppListItem* mock_app = model_updater->FindItem(app_id);
  // |mock_app| shouldn't be found in |AppList| because it should be hidden in
  // launcher.
  if (provider == ProviderType::kWebApps) {
    // TODO(crbug.com/877898): |mock_app| should be hidden but web_apps.cc does
    // not currently read from system_web_app_manager.cc. When
    // DesktopPWAsWithoutExtensions launches this should change to
    // EXPECT_FALSE().
    EXPECT_TRUE(mock_app);
  } else {
    EXPECT_FALSE(mock_app);
  }
#endif  // defined(OS_CHROMEOS)
}

class SystemWebAppManagerNotShownInSearchTest
    : public SystemWebAppManagerBrowserTest {
 public:
  SystemWebAppManagerNotShownInSearchTest()
      : SystemWebAppManagerBrowserTest(/*install_mock=*/false) {
    maybe_installation_ =
        TestSystemWebAppInstallation::SetUpAppNotShownInSearch();
  }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerNotShownInSearchTest,
                       NotShownInSearch) {
  const web_app::ProviderType provider = provider_type();

  WaitForSystemAppInstallAndLaunch(GetMockAppType());
  AppId app_id = GetManager().GetAppIdForSystemApp(GetMockAppType()).value();

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
  proxy->AppRegistryCache().ForOneApp(
      app_id, [provider](const apps::AppUpdate& update) {
        if (provider == ProviderType::kWebApps) {
          // TODO(crbug.com/877898): |mock_app| should be hidden but web_apps.cc
          // does not currently read from system_web_app_manager.cc. When
          // DesktopPWAsWithoutExtensions launches remove the special case for
          // kWebApps, ShowInSearch() should return false.
          EXPECT_EQ(apps::mojom::OptionalBool::kTrue, update.ShowInSearch());
        } else {
          EXPECT_EQ(apps::mojom::OptionalBool::kFalse, update.ShowInSearch());
        }
      });
}

class SystemWebAppManagerAdditionalSearchTermsTest
    : public SystemWebAppManagerBrowserTest {
 public:
  SystemWebAppManagerAdditionalSearchTermsTest()
      : SystemWebAppManagerBrowserTest(/*install_mock=*/false) {
    maybe_installation_ =
        TestSystemWebAppInstallation::SetUpAppWithAdditionalSearchTerms();
  }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerAdditionalSearchTermsTest,
                       AdditionalSearchTerms) {
  WaitForSystemAppInstallAndLaunch(GetMockAppType());
  AppId app_id = GetManager().GetAppIdForSystemApp(GetMockAppType()).value();

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
  proxy->AppRegistryCache().ForOneApp(
      app_id, [](const apps::AppUpdate& update) {
        EXPECT_EQ(std::vector<std::string>({"Security"}),
                  update.AdditionalSearchTerms());
      });
}

class SystemWebAppManagerChromeUntrustedTest
    : public SystemWebAppManagerBrowserTest {
 public:
  SystemWebAppManagerChromeUntrustedTest()
      : SystemWebAppManagerBrowserTest(/*install_mock=*/false) {
    maybe_installation_ =
        TestSystemWebAppInstallation::SetUpChromeUntrustedApp();
  }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerChromeUntrustedTest, Install) {
  Browser* app_browser = WaitForSystemAppInstallAndLaunch(GetMockAppType());
  AppId app_id = GetManager().GetAppIdForSystemApp(GetMockAppType()).value();
  EXPECT_EQ(app_id, app_browser->app_controller()->GetAppId());
  EXPECT_TRUE(GetManager().IsSystemWebApp(app_id));

  Profile* profile = app_browser->profile();
  AppRegistrar& registrar =
      WebAppProviderBase::GetProviderBase(profile)->registrar();

  EXPECT_EQ("Test System App", registrar.GetAppShortName(app_id));
  EXPECT_EQ(SkColorSetRGB(0, 0xFF, 0), registrar.GetAppThemeColor(app_id));
  EXPECT_TRUE(registrar.HasExternalAppWithInstallSource(
      app_id, web_app::ExternalInstallSource::kSystemInstalled));
  EXPECT_EQ(registrar.FindAppWithUrlInScope(
                GURL("chrome-untrusted://test-system-app/")),
            app_id);
}

class SystemWebAppManagerOriginTrialsBrowserTest
    : public SystemWebAppManagerBrowserTest {
 public:
  SystemWebAppManagerOriginTrialsBrowserTest()
      : SystemWebAppManagerBrowserTest(/*install_mock=*/false) {
    maybe_installation_ =
        TestSystemWebAppInstallation::SetUpAppWithEnabledOriginTrials(
            OriginTrialsMap({{GetOrigin(main_url_), main_url_trials_},
                             {GetOrigin(trial_url_), trial_url_trials_}}));
  }

  ~SystemWebAppManagerOriginTrialsBrowserTest() override = default;

 protected:
  class MockNavigationHandle : public content::MockNavigationHandle {
   public:
    explicit MockNavigationHandle(const GURL& url)
        : content::MockNavigationHandle(url, nullptr) {}
    bool IsInMainFrame() override { return is_in_main_frame_; }

    void set_is_in_main_frame(bool is_in_main_frame) {
      is_in_main_frame_ = is_in_main_frame;
    }

   private:
    bool is_in_main_frame_;
  };

  std::unique_ptr<content::WebContents> CreateTestWebContents() {
    content::WebContents::CreateParams create_params(browser()->profile());
    return content::WebContents::Create(create_params);
  }

  const std::vector<std::string> main_url_trials_ = {"Frobulate"};
  const std::vector<std::string> trial_url_trials_ = {"FrobulateNavigation"};

  const GURL main_url_ = GURL("chrome://test-system-app/pwa.html");
  const GURL trial_url_ = GURL("chrome://test-subframe/title2.html");
  const GURL notrial_url_ = GURL("chrome://notrial-subframe/title3.html");

 private:
  url::Origin GetOrigin(const GURL& url) { return url::Origin::Create(url); }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerOriginTrialsBrowserTest,
                       ForceEnabledOriginTrials_FirstNavigationIntoPage) {
  WaitForTestSystemAppInstall();

  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  WebAppTabHelper tab_helper(web_contents.get());

  // Simulate when first navigating into app's launch url.
  {
    MockNavigationHandle mock_nav_handle(main_url_);
    mock_nav_handle.set_is_in_main_frame(true);
    mock_nav_handle.set_is_same_document(false);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials(main_url_trials_));
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
    ASSERT_EQ(maybe_installation_->GetAppId(), tab_helper.GetAppId());
  }

  // Simulate loading app's embedded child-frame that has origin trials.
  {
    MockNavigationHandle mock_nav_handle(trial_url_);
    mock_nav_handle.set_is_in_main_frame(false);
    mock_nav_handle.set_is_same_document(false);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials(trial_url_trials_));
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
  }

  // Simulate loading app's embedded child-frame that has no origin trial.
  {
    MockNavigationHandle mock_nav_handle(notrial_url_);
    mock_nav_handle.set_is_in_main_frame(false);
    mock_nav_handle.set_is_same_document(false);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials).Times(0);
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
  }
}

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerOriginTrialsBrowserTest,
                       ForceEnabledOriginTrials_IntraDocumentNavigation) {
  WaitForTestSystemAppInstall();

  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  WebAppTabHelper tab_helper(web_contents.get());

  // Simulate when first navigating into app's launch url.
  {
    MockNavigationHandle mock_nav_handle(main_url_);
    mock_nav_handle.set_is_in_main_frame(true);
    mock_nav_handle.set_is_same_document(false);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials(main_url_trials_));
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
    ASSERT_EQ(maybe_installation_->GetAppId(), tab_helper.GetAppId());
  }

  // Simulate same-document navigation.
  {
    MockNavigationHandle mock_nav_handle(main_url_);
    mock_nav_handle.set_is_in_main_frame(true);
    mock_nav_handle.set_is_same_document(true);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials).Times(0);
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
  }
}

// This test checks origin trials are correctly enabled for navigations on the
// main frame, this test checks:
// - The app's main page |main_url_| has OT.
// - The iframe page |trial_url_| has OT, only if it is embedded by the app.
// - When navigating from a cross-origin page to the app's main page, the main
// page has OT.
IN_PROC_BROWSER_TEST_P(SystemWebAppManagerOriginTrialsBrowserTest,
                       ForceEnabledOriginTrials_Navigation) {
  WaitForTestSystemAppInstall();

  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  WebAppTabHelper tab_helper(web_contents.get());

  // Simulate when first navigating into app's launch url.
  {
    MockNavigationHandle mock_nav_handle(main_url_);
    mock_nav_handle.set_is_in_main_frame(true);
    mock_nav_handle.set_is_same_document(false);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials(main_url_trials_));
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
    ASSERT_EQ(maybe_installation_->GetAppId(), tab_helper.GetAppId());
  }

  // Simulate navigating to a different site without origin trials.
  {
    MockNavigationHandle mock_nav_handle(notrial_url_);
    mock_nav_handle.set_is_in_main_frame(true);
    mock_nav_handle.set_is_same_document(false);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials).Times(0);
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
    ASSERT_EQ("", tab_helper.GetAppId());
  }

  // Simulatenavigating back to a SWA with origin trials.
  {
    MockNavigationHandle mock_nav_handle(main_url_);
    mock_nav_handle.set_is_in_main_frame(true);
    mock_nav_handle.set_is_same_document(false);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials(main_url_trials_));
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
    ASSERT_EQ(maybe_installation_->GetAppId(), tab_helper.GetAppId());
  }

  // Simulate navigating the main frame to a url embedded by SWA. This url has
  // origin trials when embedded by SWA. However, when this url is loaded in the
  // main frame, it should not get origin trials.
  {
    MockNavigationHandle mock_nav_handle(trial_url_);
    mock_nav_handle.set_is_in_main_frame(true);
    mock_nav_handle.set_is_same_document(false);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials).Times(0);
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
    ASSERT_EQ("", tab_helper.GetAppId());
  }
}

// We test with and without enabling kDesktopPWAsWithoutExtensions.

INSTANTIATE_TEST_SUITE_P(All,
                         SystemWebAppManagerBrowserTest,
                         ::testing::Values(ProviderType::kBookmarkApps,
                                           ProviderType::kWebApps),
                         ProviderTypeParamToString);

INSTANTIATE_TEST_SUITE_P(
    All,
    SystemWebAppManagerLaunchFilesBrowserTest,
    testing::Combine(
        /* enable_origin_scoped_permission_context */ testing::Bool(),
        /* enable_pwas_without_extensions */ testing::Bool()));

INSTANTIATE_TEST_SUITE_P(
    All,
    SystemWebAppManagerLaunchDirectoryBrowserTest,
    testing::Combine(
        /* enable_origin_scoped_permission_context */ testing::Bool(),
        /* enable_desktop_pwas_without_extensions */ testing::Bool()));

INSTANTIATE_TEST_SUITE_P(All,
                         SystemWebAppManagerNotShownInLauncherTest,
                         ::testing::Values(ProviderType::kBookmarkApps,
                                           ProviderType::kWebApps),
                         ProviderTypeParamToString);

INSTANTIATE_TEST_SUITE_P(All,
                         SystemWebAppManagerNotShownInSearchTest,
                         ::testing::Values(ProviderType::kBookmarkApps,
                                           ProviderType::kWebApps),
                         ProviderTypeParamToString);

INSTANTIATE_TEST_SUITE_P(All,
                         SystemWebAppManagerAdditionalSearchTermsTest,
                         ::testing::Values(ProviderType::kBookmarkApps,
                                           ProviderType::kWebApps),
                         ProviderTypeParamToString);

INSTANTIATE_TEST_SUITE_P(All,
                         SystemWebAppManagerChromeUntrustedTest,
                         ::testing::Values(ProviderType::kBookmarkApps,
                                           ProviderType::kWebApps),
                         ProviderTypeParamToString);

INSTANTIATE_TEST_SUITE_P(All,
                         SystemWebAppManagerOriginTrialsBrowserTest,
                         ::testing::Values(ProviderType::kBookmarkApps,
                                           ProviderType::kWebApps),
                         ProviderTypeParamToString);

INSTANTIATE_TEST_SUITE_P(All,
                         SystemWebAppManagerFileHandlingOriginTrialsBrowserTest,
                         ::testing::Values(ProviderType::kBookmarkApps,
                                           ProviderType::kWebApps),
                         ProviderTypeParamToString);

}  // namespace web_app
