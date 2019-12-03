// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/system_web_app_manager_browsertest.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
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
#include "chrome/browser/web_applications/test/test_system_web_app_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/manifest_handlers/app_theme_color_info.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace web_app {

namespace {

constexpr char kSystemAppManifestText[] =
    R"({
      "name": "Test System App",
      "display": "standalone",
      "icons": [
        {
          "src": "icon-256.png",
          "sizes": "256x256",
          "type": "image/png"
        }
      ],
      "start_url": "/pwa.html",
      "theme_color": "#00FF00"
    })";

constexpr char kPwaHtml[] =
    R"(
<html>
<head>
  <link rel="manifest" href="manifest.json">
</head>
</html>
)";

// WebUIController that serves a System PWA.
class TestWebUIController : public content::WebUIController {
 public:
  explicit TestWebUIController(content::WebUI* web_ui)
      : WebUIController(web_ui) {
    content::WebUIDataSource* data_source =
        content::WebUIDataSource::Create("test-system-app");
    data_source->AddResourcePath("icon-256.png", IDR_PRODUCT_LOGO_256);
    data_source->SetRequestFilter(
        base::BindRepeating([](const std::string& path) {
          return path == "manifest.json" || path == "pwa.html";
        }),
        base::BindRepeating(
            [](const std::string& id,
               content::WebUIDataSource::GotDataCallback callback) {
              scoped_refptr<base::RefCountedString> ref_contents(
                  new base::RefCountedString);
              if (id == "manifest.json")
                ref_contents->data() = kSystemAppManifestText;
              else if (id == "pwa.html")
                ref_contents->data() = kPwaHtml;
              else
                NOTREACHED();

              std::move(callback).Run(ref_contents);
            }));
    content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                  data_source);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWebUIController);
};

}  // namespace

// WebUIControllerFactory that serves our TestWebUIController.
class TestWebUIControllerFactory : public content::WebUIControllerFactory {
 public:
  TestWebUIControllerFactory() {}

  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override {
    return std::make_unique<TestWebUIController>(web_ui);
  }

  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) override {
    if (url.SchemeIs(content::kChromeUIScheme))
      return reinterpret_cast<content::WebUI::TypeID>(1);

    return content::WebUI::kNoWebUI;
  }

  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) override {
    return url.SchemeIs(content::kChromeUIScheme);
  }
  bool UseWebUIBindingsForURL(content::BrowserContext* browser_context,
                              const GURL& url) override {
    return url.SchemeIs(content::kChromeUIScheme);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWebUIControllerFactory);
};

SystemWebAppManagerBrowserTest::SystemWebAppManagerBrowserTest(
    bool install_mock) {
  scoped_feature_list_.InitWithFeatures(
      {features::kSystemWebApps, blink::features::kNativeFileSystemAPI,
       blink::features::kFileHandlingAPI},
      {});
  if (install_mock) {
    factory_ = std::make_unique<TestWebUIControllerFactory>();
    test_web_app_provider_creator_ =
        std::make_unique<TestWebAppProviderCreator>(base::BindOnce(
            &SystemWebAppManagerBrowserTest::CreateWebAppProvider,
            base::Unretained(this)));
    content::WebUIControllerFactory::RegisterFactory(factory_.get());
  }
}

SystemWebAppManagerBrowserTest::~SystemWebAppManagerBrowserTest() {
  if (factory_) {
    content::WebUIControllerFactory::UnregisterFactoryForTesting(
        factory_.get());
  }
}

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

std::unique_ptr<KeyedService>
SystemWebAppManagerBrowserTest::CreateWebAppProvider(Profile* profile) {
  DCHECK(SystemWebAppManager::IsEnabled());

  auto provider = std::make_unique<TestWebAppProvider>(profile);

  // Override SystemWebAppManager with TestSystemWebAppManager:
  DCHECK(!test_system_web_app_manager_);
  auto test_system_web_app_manager =
      std::make_unique<TestSystemWebAppManager>(profile);
  test_system_web_app_manager_ = test_system_web_app_manager.get();
  provider->SetSystemWebAppManager(std::move(test_system_web_app_manager));

  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(
      SystemAppType::SETTINGS,
      SystemAppInfo("OSSettings",
                    content::GetWebUIURL("test-system-app/pwa.html")));
  test_system_web_app_manager_->SetSystemApps(std::move(system_apps));

  // Start registry and all dependent subsystems:
  provider->Start();

  return provider;
}

void SystemWebAppManagerBrowserTest::WaitForTestSystemAppInstall() {
  // Wait for the System Web Apps to install.
  if (factory_) {
    base::RunLoop run_loop;
    GetManager().on_apps_synchronized().Post(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
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

// Test that System Apps install correctly with a manifest.
IN_PROC_BROWSER_TEST_F(SystemWebAppManagerBrowserTest, Install) {
  const extensions::Extension* app = GetExtensionForAppBrowser(
      WaitForSystemAppInstallAndLaunch(SystemAppType::SETTINGS));
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
  Browser* app_browser =
      WaitForSystemAppInstallAndLaunch(SystemAppType::SETTINGS);
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
  apps::AppLaunchParams params = LaunchParamsForApp(SystemAppType::SETTINGS);
  params.source = apps::mojom::AppLaunchSource::kSourceChromeInternal;

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  base::FilePath temp_file_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_directory.GetPath(),
                                             &temp_file_path));

  params.launch_files = {temp_file_path};

  const GURL& launch_url = WebAppProvider::Get(browser()->profile())
                               ->registrar()
                               .GetAppLaunchURL(params.app_id);
  content::TestNavigationObserver navigation_observer(launch_url);
  navigation_observer.StartWatchingNewWebContents();

  content::WebContents* web_contents =
      OpenApplication(browser()->profile(), params);

  navigation_observer.Wait();

  auto result =
      content::EvalJs(web_contents,
                      "new Promise(resolve => {"
                      "  launchQueue.setConsumer(launchParams => {"
                      "    resolve(launchParams.files.length);"
                      "  });"
                      "});",
                      content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /*world_id*/);

  // Files array should be populated with one file.
  EXPECT_EQ(1, result.value.GetInt());
}

}  // namespace web_app
