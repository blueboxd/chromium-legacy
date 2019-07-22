// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/pending_bookmark_app_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {

web_app::ExternalInstallOptions CreateInstallOptions(const GURL& url) {
  web_app::ExternalInstallOptions install_options(
      url, web_app::LaunchContainer::kWindow,
      web_app::ExternalInstallSource::kInternalDefault);
  // Avoid creating real shortcuts in tests.
  install_options.add_to_applications_menu = false;
  install_options.add_to_desktop = false;
  install_options.add_to_quick_launch_bar = false;

  return install_options;
}

class PendingBookmarkAppManagerBrowserTest : public InProcessBrowserTest {
 protected:
  void InstallApp(web_app::ExternalInstallOptions install_options) {
    base::RunLoop run_loop;

    web_app::WebAppProvider::Get(browser()->profile())
        ->pending_app_manager()
        .Install(std::move(install_options),
                 base::BindLambdaForTesting(
                     [this, &run_loop](const GURL& provided_url,
                                       web_app::InstallResultCode code) {
                       result_code_ = code;
                       run_loop.Quit();
                     }));
    run_loop.Run();
    ASSERT_TRUE(result_code_.has_value());
  }

  base::Optional<web_app::InstallResultCode> result_code_;
};

// Basic integration test to make sure the whole flow works. Each step in the
// flow is unit tested separately.
IN_PROC_BROWSER_TEST_F(PendingBookmarkAppManagerBrowserTest, InstallSucceeds) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/banners/manifest_test_page.html"));
  InstallApp(CreateInstallOptions(url));
  EXPECT_EQ(web_app::InstallResultCode::kSuccess, result_code_.value());
  base::Optional<web_app::AppId> id =
      web_app::ExternallyInstalledWebAppPrefs(browser()->profile()->GetPrefs())
          .LookupAppId(url);
  ASSERT_TRUE(id.has_value());
  const Extension* app = ExtensionRegistry::Get(browser()->profile())
                             ->enabled_extensions()
                             .GetByID(id.value());
  ASSERT_TRUE(app);
  EXPECT_EQ("Manifest test app", app->name());
}

// Tests that the browser doesn't crash if it gets shutdown with a pending
// installation.
IN_PROC_BROWSER_TEST_F(PendingBookmarkAppManagerBrowserTest,
                       ShutdownWithPendingInstallation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  web_app::ExternalInstallOptions install_options = CreateInstallOptions(
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"));

  // Start an installation but don't wait for it to finish.
  web_app::WebAppProvider::Get(browser()->profile())
      ->pending_app_manager()
      .Install(std::move(install_options), base::DoNothing());

  // The browser should shutdown cleanly even if there is a pending
  // installation.
}

IN_PROC_BROWSER_TEST_F(PendingBookmarkAppManagerBrowserTest,
                       BypassServiceWorkerCheck) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(
      "/banners/manifest_no_service_worker.html"));

  web_app::ExternalInstallOptions install_options = CreateInstallOptions(url);
  install_options.bypass_service_worker_check = true;
  InstallApp(std::move(install_options));
  const extensions::Extension* app =
      extensions::util::GetInstalledPwaForUrl(browser()->profile(), url);
  EXPECT_TRUE(app);
  EXPECT_EQ("Manifest test app", app->name());
}

IN_PROC_BROWSER_TEST_F(PendingBookmarkAppManagerBrowserTest,
                       PerformServiceWorkerCheck) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(
      "/banners/manifest_no_service_worker.html"));
  web_app::ExternalInstallOptions install_options = CreateInstallOptions(url);
  InstallApp(std::move(install_options));
  const extensions::Extension* app =
      extensions::util::GetInstalledPwaForUrl(browser()->profile(), url);
  EXPECT_FALSE(app);
}

IN_PROC_BROWSER_TEST_F(PendingBookmarkAppManagerBrowserTest, ForceReinstall) {
  ASSERT_TRUE(embedded_test_server()->Start());
  {
    GURL url(embedded_test_server()->GetURL(
        "/banners/"
        "manifest_test_page.html?manifest=manifest_short_name_only.json"));
    web_app::ExternalInstallOptions install_options = CreateInstallOptions(url);
    install_options.force_reinstall = true;
    InstallApp(std::move(install_options));

    const extensions::Extension* app =
        extensions::util::GetInstalledPwaForUrl(browser()->profile(), url);
    EXPECT_TRUE(app);
    EXPECT_EQ("Manifest", app->name());
  }
  {
    GURL url(
        embedded_test_server()->GetURL("/banners/manifest_test_page.html"));
    web_app::ExternalInstallOptions install_options = CreateInstallOptions(url);
    install_options.force_reinstall = true;
    InstallApp(std::move(install_options));

    const extensions::Extension* app =
        extensions::util::GetInstalledPwaForUrl(browser()->profile(), url);
    EXPECT_TRUE(app);
    EXPECT_EQ("Manifest test app", app->name());
  }
}

// Test that adding a manifest that points to a chrome:// URL does not actually
// install a bookmark app that points to a chrome:// URL.
IN_PROC_BROWSER_TEST_F(PendingBookmarkAppManagerBrowserTest,
                       InstallChromeURLFails) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(
      "/banners/manifest_test_page.html?manifest=manifest_chrome_url.json"));
  InstallApp(CreateInstallOptions(url));
  EXPECT_EQ(web_app::InstallResultCode::kSuccess, result_code_.value());
  base::Optional<web_app::AppId> id =
      web_app::ExternallyInstalledWebAppPrefs(browser()->profile()->GetPrefs())
          .LookupAppId(url);
  ASSERT_TRUE(id.has_value());
  const Extension* app = ExtensionRegistry::Get(browser()->profile())
                             ->enabled_extensions()
                             .GetByID(id.value());
  ASSERT_TRUE(app);

  // The installer falls back to installing a bookmark app of the original URL.
  EXPECT_EQ(url, extensions::AppLaunchInfo::GetLaunchWebURL(app));
  EXPECT_FALSE(extensions::UrlHandlers::CanBookmarkAppHandleUrl(
      app, GURL("chrome://settings")));
}

// Test that adding a web app without a manifest while using the
// |require_manifest| flag fails.
IN_PROC_BROWSER_TEST_F(PendingBookmarkAppManagerBrowserTest,
                       RequireManifestFailsIfNoManifest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html"));
  web_app::ExternalInstallOptions install_options = CreateInstallOptions(url);
  install_options.require_manifest = true;
  InstallApp(std::move(install_options));

  EXPECT_EQ(web_app::InstallResultCode::kNotValidManifestForWebApp,
            result_code_.value());
  base::Optional<web_app::AppId> id =
      web_app::ExternallyInstalledWebAppPrefs(browser()->profile()->GetPrefs())
          .LookupAppId(url);
  ASSERT_FALSE(id.has_value());
}

}  // namespace extensions
