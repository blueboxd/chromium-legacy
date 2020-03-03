// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_migration_manager.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/ssl_test_utils.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/web_applications/components/app_icon_manager.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_shortcut_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/ssl/ssl_info.h"

namespace web_app {

namespace {

constexpr char kBaseDataDir[] = "chrome/test/data/banners";

// start_url in manifest.json matches navigation url for the simple
// manifest_test_page.html.
constexpr char kSimpleManifestStartUrl[] =
    "https://example.org/manifest_test_page.html";

// Performs blocking IO operations.
base::FilePath GetDataFilePath(const base::FilePath& relative_path,
                               bool* path_exists) {
  base::ScopedAllowBlockingForTesting allow_io;

  base::FilePath root_path;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &root_path));
  base::FilePath path = root_path.Append(relative_path);
  *path_exists = base::PathExists(path);
  return path;
}

}  // namespace

class WebAppMigrationManagerBrowserTest : public InProcessBrowserTest {
 public:
  WebAppMigrationManagerBrowserTest() {
    if (content::IsPreTest()) {
      scoped_feature_list_.InitAndDisableFeature(
          features::kDesktopPWAsWithoutExtensions);
    } else {
      scoped_feature_list_.InitAndEnableFeature(
          features::kDesktopPWAsWithoutExtensions);
    }
  }

  ~WebAppMigrationManagerBrowserTest() override = default;

  WebAppMigrationManagerBrowserTest(const WebAppMigrationManagerBrowserTest&) =
      delete;
  WebAppMigrationManagerBrowserTest& operator=(
      const WebAppMigrationManagerBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // We use a URLLoaderInterceptor, rather than the EmbeddedTestServer, since
    // a stable app_id across tests requires stable origin, whereas
    // EmbeddedTestServer serves content on a random port.
    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            [](content::URLLoaderInterceptor::RequestParams* params) -> bool {
              std::string relative_request = base::StrCat(
                  {kBaseDataDir, params->url_request.url.path_piece()});
              base::FilePath relative_path =
                  base::FilePath().AppendASCII(relative_request);

              bool path_exists = false;
              base::FilePath path =
                  GetDataFilePath(relative_path, &path_exists);
              if (!path_exists)
                return /*intercepted=*/false;

              // Provide fake SSLInfo to avoid NOT_FROM_SECURE_ORIGIN error in
              // InstallableManager::GetData().
              net::SSLInfo ssl_info;
              CreateFakeSslInfoCertificate(&ssl_info);

              content::URLLoaderInterceptor::WriteResponse(
                  path, params->client.get(), /*headers=*/nullptr, ssl_info);

              return /*intercepted=*/true;
            }));
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  AppId InstallWebAppAsUserViaOmnibox() {
    chrome::SetAutoAcceptPWAInstallConfirmationForTesting(true);
    chrome::SetAutoAcceptWebAppDialogForTesting(
        /*auto_accept=*/true,
        /*auto_open_in_window=*/true);

    provider().shortcut_manager().SuppressShortcutsForTesting();

    AppId app_id;
    base::RunLoop run_loop;
    bool started = CreateWebAppFromManifest(
        browser()->tab_strip_model()->GetActiveWebContents(),
        WebappInstallSource::OMNIBOX_INSTALL_ICON,
        base::BindLambdaForTesting(
            [&](const AppId& installed_app_id, InstallResultCode code) {
              EXPECT_EQ(code, InstallResultCode::kSuccessNewInstall);
              app_id = installed_app_id;
              run_loop.Quit();
            }));
    EXPECT_TRUE(started);
    run_loop.Run();
    return app_id;
  }

  WebAppProvider& provider() {
    WebAppProvider* provider = WebAppProvider::Get(browser()->profile());
    DCHECK(provider);
    return *provider;
  }

  void AwaitRegistryReady() {
    base::RunLoop run_loop;
    provider().on_registry_ready().Post(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppMigrationManagerBrowserTest,
                       PRE_DatabaseMigration_SimpleManifest) {
  ui_test_utils::NavigateToURL(browser(), GURL{kSimpleManifestStartUrl});
  AppId app_id = InstallWebAppAsUserViaOmnibox();
  EXPECT_EQ(GenerateAppIdFromURL(GURL{kSimpleManifestStartUrl}), app_id);

  EXPECT_TRUE(provider().registrar().AsBookmarkAppRegistrar());
  EXPECT_FALSE(provider().registrar().AsWebAppRegistrar());

  EXPECT_TRUE(provider().registrar().IsInstalled(app_id));
}

IN_PROC_BROWSER_TEST_F(WebAppMigrationManagerBrowserTest,
                       DatabaseMigration_SimpleManifest) {
  AwaitRegistryReady();

  AppId app_id = GenerateAppIdFromURL(GURL{kSimpleManifestStartUrl});
  EXPECT_TRUE(provider().registrar().IsInstalled(app_id));

  WebAppRegistrar* registrar = provider().registrar().AsWebAppRegistrar();
  ASSERT_TRUE(registrar);
  EXPECT_FALSE(provider().registrar().AsBookmarkAppRegistrar());

  const WebApp* web_app = registrar->GetAppById(app_id);
  ASSERT_TRUE(web_app);

  EXPECT_EQ("Manifest test app", web_app->name());
  EXPECT_EQ(DisplayMode::kStandalone, web_app->display_mode());

  const std::vector<SquareSizePx> icon_sizes_in_px = {32,  48,  64,  96, 128,
                                                      144, 192, 256, 512};
  EXPECT_EQ(icon_sizes_in_px, web_app->downloaded_icon_sizes());

  base::RunLoop run_loop;
  provider().icon_manager().ReadIcons(
      app_id, web_app->downloaded_icon_sizes(),
      base::BindLambdaForTesting(
          [&](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
            EXPECT_EQ(9u, icon_bitmaps.size());
            for (auto& size_px_and_bitmap : icon_bitmaps) {
              SquareSizePx size_px = size_px_and_bitmap.first;
              EXPECT_TRUE(base::Contains(icon_sizes_in_px, size_px));

              SkBitmap bitmap = size_px_and_bitmap.second;
              EXPECT_FALSE(bitmap.empty());
              EXPECT_EQ(size_px, bitmap.width());
              EXPECT_EQ(size_px, bitmap.height());
            }
            run_loop.Quit();
          }));
  run_loop.Run();
}

// TODO(crbug.com/1020037): Test policy installed bookmark apps with an external
// install source to cover
// WebAppMigrationManager::MigrateBookmarkAppInstallSource() logic.

}  // namespace web_app
