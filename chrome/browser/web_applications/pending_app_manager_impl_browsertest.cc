// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/pending_app_manager_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/url_constants.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace web_app {

ExternalInstallOptions CreateInstallOptions(const GURL& url) {
  ExternalInstallOptions install_options(
      url, LaunchContainer::kWindow, ExternalInstallSource::kInternalDefault);
  // Avoid creating real shortcuts in tests.
  install_options.add_to_applications_menu = false;
  install_options.add_to_desktop = false;
  install_options.add_to_quick_launch_bar = false;

  return install_options;
}

class PendingAppManagerImplBrowserTest : public InProcessBrowserTest {
 protected:
  AppRegistrar& registrar() {
    return WebAppProviderBase::GetProviderBase(browser()->profile())
        ->registrar();
  }

  void InstallApp(ExternalInstallOptions install_options) {
    base::RunLoop run_loop;

    WebAppProviderBase::GetProviderBase(browser()->profile())
        ->pending_app_manager()
        .Install(std::move(install_options),
                 base::BindLambdaForTesting(
                     [this, &run_loop](const GURL& provided_url,
                                       InstallResultCode code) {
                       result_code_ = code;
                       run_loop.Quit();
                     }));
    run_loop.Run();
    ASSERT_TRUE(result_code_.has_value());
  }

  base::Optional<InstallResultCode> result_code_;
};

// Basic integration test to make sure the whole flow works. Each step in the
// flow is unit tested separately.
IN_PROC_BROWSER_TEST_F(PendingAppManagerImplBrowserTest, InstallSucceeds) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/banners/manifest_test_page.html"));
  InstallApp(CreateInstallOptions(url));
  EXPECT_EQ(InstallResultCode::kSuccess, result_code_.value());
  base::Optional<AppId> app_id =
      ExternallyInstalledWebAppPrefs(browser()->profile()->GetPrefs())
          .LookupAppId(url);
  EXPECT_TRUE(app_id.has_value());
  EXPECT_EQ("Manifest test app", registrar().GetAppShortName(app_id.value()));
}

// Tests that the browser doesn't crash if it gets shutdown with a pending
// installation.
IN_PROC_BROWSER_TEST_F(PendingAppManagerImplBrowserTest,
                       ShutdownWithPendingInstallation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ExternalInstallOptions install_options = CreateInstallOptions(
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"));

  // Start an installation but don't wait for it to finish.
  WebAppProviderBase::GetProviderBase(browser()->profile())
      ->pending_app_manager()
      .Install(std::move(install_options), base::DoNothing());

  // The browser should shutdown cleanly even if there is a pending
  // installation.
}

IN_PROC_BROWSER_TEST_F(PendingAppManagerImplBrowserTest,
                       BypassServiceWorkerCheck) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(
      "/banners/manifest_no_service_worker.html"));

  ExternalInstallOptions install_options = CreateInstallOptions(url);
  install_options.bypass_service_worker_check = true;
  InstallApp(std::move(install_options));
  base::Optional<AppId> app_id = registrar().FindAppWithUrlInScope(url);
  EXPECT_TRUE(app_id.has_value());
  EXPECT_TRUE(registrar().GetAppScope(*app_id).has_value());
  EXPECT_EQ("Manifest test app", registrar().GetAppShortName(*app_id));
}

IN_PROC_BROWSER_TEST_F(PendingAppManagerImplBrowserTest,
                       PerformServiceWorkerCheck) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(
      "/banners/manifest_no_service_worker.html"));
  ExternalInstallOptions install_options = CreateInstallOptions(url);
  InstallApp(std::move(install_options));
  base::Optional<AppId> app_id = registrar().FindAppWithUrlInScope(url);
  EXPECT_TRUE(app_id.has_value());
  EXPECT_FALSE(registrar().GetAppScope(app_id.value()).has_value());
}

IN_PROC_BROWSER_TEST_F(PendingAppManagerImplBrowserTest, ForceReinstall) {
  ASSERT_TRUE(embedded_test_server()->Start());
  {
    GURL url(embedded_test_server()->GetURL(
        "/banners/"
        "manifest_test_page.html?manifest=manifest_short_name_only.json"));
    ExternalInstallOptions install_options = CreateInstallOptions(url);
    install_options.force_reinstall = true;
    InstallApp(std::move(install_options));

    base::Optional<AppId> app_id = registrar().FindAppWithUrlInScope(url);
    EXPECT_TRUE(app_id.has_value());
    EXPECT_EQ("Manifest", registrar().GetAppShortName(app_id.value()));
  }
  {
    GURL url(
        embedded_test_server()->GetURL("/banners/manifest_test_page.html"));
    ExternalInstallOptions install_options = CreateInstallOptions(url);
    install_options.force_reinstall = true;
    InstallApp(std::move(install_options));

    base::Optional<AppId> app_id = registrar().FindAppWithUrlInScope(url);
    EXPECT_TRUE(app_id.has_value());
    EXPECT_EQ("Manifest test app", registrar().GetAppShortName(app_id.value()));
  }
}

// Test that adding a manifest that points to a chrome:// URL does not actually
// install a web app that points to a chrome:// URL.
IN_PROC_BROWSER_TEST_F(PendingAppManagerImplBrowserTest,
                       InstallChromeURLFails) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(
      "/banners/manifest_test_page.html?manifest=manifest_chrome_url.json"));
  InstallApp(CreateInstallOptions(url));
  EXPECT_EQ(InstallResultCode::kSuccess, result_code_.value());
  base::Optional<AppId> app_id =
      ExternallyInstalledWebAppPrefs(browser()->profile()->GetPrefs())
          .LookupAppId(url);
  ASSERT_TRUE(app_id.has_value());

  // The installer falls back to installing a web app of the original URL.
  EXPECT_EQ(url, registrar().GetAppLaunchURL(app_id.value()));
  EXPECT_NE(app_id,
            registrar().FindAppWithUrlInScope(GURL("chrome://settings")));
}

// Test that adding a web app without a manifest while using the
// |require_manifest| flag fails.
IN_PROC_BROWSER_TEST_F(PendingAppManagerImplBrowserTest,
                       RequireManifestFailsIfNoManifest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html"));
  ExternalInstallOptions install_options = CreateInstallOptions(url);
  install_options.require_manifest = true;
  InstallApp(std::move(install_options));

  EXPECT_EQ(InstallResultCode::kNotValidManifestForWebApp,
            result_code_.value());
  base::Optional<AppId> id =
      ExternallyInstalledWebAppPrefs(browser()->profile()->GetPrefs())
          .LookupAppId(url);
  ASSERT_FALSE(id.has_value());
}

}  // namespace web_app
