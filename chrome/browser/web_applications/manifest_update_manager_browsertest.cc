// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/manifest_update_manager.h"

#include <string>
#include <vector>

#include "base/bind_helpers.h"
#include "base/strings/string_util.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/web_applications/components/app_icon_manager.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/test/test_system_web_app_installation.h"
#include "chrome/browser/web_applications/test/web_app_install_observer.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {

namespace {

constexpr char kUpdateHistogramName[] = "Webapp.Update.ManifestUpdateResult";

constexpr char kInstallableIconList[] = R"(
  [
    {
      "src": "launcher-icon-4x.png",
      "sizes": "192x192",
      "type": "image/png"
    }
  ]
)";

constexpr char kAnotherInstallableIconList[] = R"(
  [
    {
      "src": "/banners/image-512px.png",
      "sizes": "512x512",
      "type": "image/png"
    }
  ]
)";

ManifestUpdateManager& GetManifestUpdateManager(Browser* browser) {
  return WebAppProviderBase::GetProviderBase(browser->profile())
      ->manifest_update_manager();
}

class UpdateCheckResultAwaiter {
 public:
  explicit UpdateCheckResultAwaiter(Browser* browser, const GURL& url)
      : browser_(browser), url_(url) {
    SetCallback();
  }

  void SetCallback() {
    GetManifestUpdateManager(browser_).SetResultCallbackForTesting(
        base::BindOnce(&UpdateCheckResultAwaiter::OnResult,
                       base::Unretained(this)));
  }

  ManifestUpdateResult AwaitNextResult() && {
    run_loop_.Run();
    return *result_;
  }

  void OnResult(const GURL& url, ManifestUpdateResult result) {
    if (url != url_) {
      SetCallback();
      return;
    }
    result_ = result;
    run_loop_.Quit();
  }

 private:
  Browser* browser_ = nullptr;
  const GURL& url_;
  base::RunLoop run_loop_;
  base::Optional<ManifestUpdateResult> result_;
};

}  // namespace

class ManifestUpdateManagerBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<ProviderType> {
 public:
  ManifestUpdateManagerBrowserTest() = default;
  ~ManifestUpdateManagerBrowserTest() override = default;

  void SetUp() override {
    if (GetParam() == ProviderType::kWebApps) {
      scoped_feature_list_.InitWithFeatures(
          {features::kDesktopPWAsLocalUpdating,
           features::kDesktopPWAsWithoutExtensions},
          {});
    } else if (GetParam() == ProviderType::kBookmarkApps) {
      scoped_feature_list_.InitWithFeatures(
          {features::kDesktopPWAsLocalUpdating},
          {features::kDesktopPWAsWithoutExtensions});
    }

    http_server_.AddDefaultHandlers(GetChromeTestDataDir());
    http_server_.RegisterRequestHandler(base::BindRepeating(
        &ManifestUpdateManagerBrowserTest::RequestHandlerOverride,
        base::Unretained(this)));
    ASSERT_TRUE(http_server_.Start());

    InProcessBrowserTest::SetUp();
  }

  std::unique_ptr<net::test_server::HttpResponse> RequestHandlerOverride(
      const net::test_server::HttpRequest& request) {
    if (request_override_)
      return request_override_.Run(request);
    return nullptr;
  }

  void OverrideManifest(const char* manifest_template,
                        const std::vector<std::string>& substitutions) {
    std::string content = base::ReplaceStringPlaceholders(
        manifest_template, substitutions, nullptr);
    request_override_ = base::BindLambdaForTesting(
        [this, content = std::move(content)](
            const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.GetURL() != GetManifestURL())
            return nullptr;
          auto http_response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          http_response->set_code(net::HTTP_FOUND);
          http_response->set_content(content);
          return std::move(http_response);
        });
  }

  GURL GetAppURL() const {
    return http_server_.GetURL("/banners/manifest_test_page.html");
  }

  GURL GetManifestURL() const {
    return http_server_.GetURL("/banners/manifest.json");
  }

  AppId InstallWebApp() {
    GURL app_url = GetAppURL();
    ui_test_utils::NavigateToURL(browser(), app_url);

    AppId app_id;
    base::RunLoop run_loop;
    InstallManager::InstallParams params;
    params.fallback_start_url = http_server_.GetURL("/fallback-url");
    params.add_to_applications_menu = false;
    params.add_to_desktop = false;
    params.add_to_quick_launch_bar = false;
    params.bypass_service_worker_check = true;
    params.require_manifest = false;
    GetProvider().install_manager().InstallWebAppWithParams(
        browser()->tab_strip_model()->GetActiveWebContents(), params,
        WebappInstallSource::OMNIBOX_INSTALL_ICON,
        base::BindLambdaForTesting(
            [&](const AppId& new_app_id, InstallResultCode code) {
              EXPECT_EQ(code, InstallResultCode::kSuccessNewInstall);
              app_id = new_app_id;
              run_loop.Quit();
            }));
    run_loop.Run();
    return app_id;
  }

  AppId InstallPolicyApp() {
    const GURL app_url = GetAppURL();
    base::RunLoop run_loop;
    ExternalInstallOptions install_options(
        app_url, DisplayMode::kStandalone,
        ExternalInstallSource::kExternalPolicy);
    install_options.add_to_applications_menu = false;
    install_options.add_to_desktop = false;
    install_options.add_to_quick_launch_bar = false;
    install_options.install_placeholder = true;
    GetProvider().pending_app_manager().Install(
        std::move(install_options),
        base::BindLambdaForTesting(
            [&](const GURL& installed_app_url, InstallResultCode code) {
              EXPECT_EQ(installed_app_url, app_url);
              EXPECT_EQ(code, InstallResultCode::kSuccessNewInstall);
              run_loop.Quit();
            }));
    run_loop.Run();
    return GetProvider().registrar().LookupExternalAppId(app_url).value();
  }

  void SetTimeOverride(base::Time time_override) {
    GetManifestUpdateManager(browser()).set_time_override_for_testing(
        time_override);
  }

  ManifestUpdateResult GetResultAfterPageLoad(const GURL& url,
                                              const AppId* app_id) {
    UpdateCheckResultAwaiter awaiter(browser(), url);
    ui_test_utils::NavigateToURL(browser(), url);
    return std::move(awaiter).AwaitNextResult();
  }

  WebAppProviderBase& GetProvider() {
    return *WebAppProviderBase::GetProviderBase(browser()->profile());
  }

 protected:
  net::EmbeddedTestServer::HandleRequestCallback request_override_;

  base::HistogramTester histogram_tester_;

  net::EmbeddedTestServer http_server_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ManifestUpdateManagerBrowserTest);
};

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest,
                       CheckOutOfScopeNavigation) {
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), nullptr),
            ManifestUpdateResult::kNoAppInScope);

  AppId app_id = InstallWebApp();

  EXPECT_EQ(GetResultAfterPageLoad(GURL("http://example.org"), nullptr),
            ManifestUpdateResult::kNoAppInScope);

  histogram_tester_.ExpectTotalCount(kUpdateHistogramName, 0);
}

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest, CheckIsThrottled) {
  constexpr base::TimeDelta kDelayBetweenChecks = base::TimeDelta::FromDays(1);
  base::Time time_override = base::Time::UnixEpoch();
  SetTimeOverride(time_override);

  AppId app_id = InstallWebApp();
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpToDate);

  time_override += kDelayBetweenChecks / 2;
  SetTimeOverride(time_override);
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kThrottled);

  time_override += kDelayBetweenChecks;
  SetTimeOverride(time_override);
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpToDate);

  time_override += kDelayBetweenChecks / 2;
  SetTimeOverride(time_override);
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kThrottled);

  time_override += kDelayBetweenChecks * 2;
  SetTimeOverride(time_override);
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpToDate);

  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kThrottled, 2);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 3);
}

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest,
                       CheckCancelledByWebContentsDestroyed) {
  AppId app_id = InstallWebApp();
  GetManifestUpdateManager(browser()).hang_update_checks_for_testing();

  GURL url = GetAppURL();
  UpdateCheckResultAwaiter awaiter(browser(), url);
  ui_test_utils::NavigateToURL(browser(), url);
  chrome::CloseTab(browser());
  EXPECT_EQ(std::move(awaiter).AwaitNextResult(),
            ManifestUpdateResult::kWebContentsDestroyed);
  histogram_tester_.ExpectBucketCount(
      kUpdateHistogramName, ManifestUpdateResult::kWebContentsDestroyed, 1);
}

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest,
                       CheckCancelledByAppUninstalled) {
  AppId app_id = InstallWebApp();
  GetManifestUpdateManager(browser()).hang_update_checks_for_testing();

  GURL url = GetAppURL();
  UpdateCheckResultAwaiter awaiter(browser(), url);
  ui_test_utils::NavigateToURL(browser(), url);
  GetProvider().install_finalizer().UninstallWebAppFromSyncByUser(
      app_id, base::DoNothing());
  EXPECT_EQ(std::move(awaiter).AwaitNextResult(),
            ManifestUpdateResult::kAppUninstalled);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUninstalled, 1);
}

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresWhitespaceDifferences) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1
      $2
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, ""});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {kInstallableIconList, "\n\n\n\n"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
}

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresNameChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "$1",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"Test app name", kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate,
                   {"Different app name", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
}

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresShortNameChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "short_name": "$1",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate,
                   {"Short test app name", kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate,
                   {"Different short test app name", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
}

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresStartUrlChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": "$1",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"a.html", kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {"b.html", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
}

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresNoManifestChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  AppId app_id = InstallWebApp();
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
}

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresInvalidManifest) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      $2
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, ""});
  AppId app_id = InstallWebApp();
  OverrideManifest(kManifestTemplate, {kInstallableIconList,
                                       "invalid manifest syntax !@#$%^*&()"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppNotEligible);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppNotEligible, 1);
}

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresNonLocalApps) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "theme_color": "$2"
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "blue"});
  AppId app_id = InstallWebApp();

  GetProvider().registry_controller().SetAppIsLocallyInstalled(app_id, false);
  EXPECT_FALSE(GetProvider().registrar().IsLocallyInstalled(app_id));

  OverrideManifest(kManifestTemplate, {kInstallableIconList, "red"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kNoAppInScope);
  histogram_tester_.ExpectTotalCount(kUpdateHistogramName, 0);
}

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresPlaceholderApps) {
  // Set up app URL to redirect to force placeholder app to install.
  const GURL app_url = GetAppURL();
  request_override_ = base::BindLambdaForTesting(
      [&app_url](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.GetURL() != app_url)
          return nullptr;
        auto http_response =
            std::make_unique<net::test_server::BasicHttpResponse>();
        http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
        http_response->AddCustomHeader(
            "Location", "http://other-origin.com/defaultresponse");
        http_response->set_content("redirect page");
        return std::move(http_response);
      });

  // Install via PendingAppManager, the redirect to a different origin should
  // cause it to install a placeholder app.
  AppId app_id = InstallPolicyApp();
  EXPECT_TRUE(GetProvider().registrar().IsPlaceholderApp(app_id));

  // Manifest updating should ignore non-redirect loads for placeholder apps
  // because the PendingAppManager will handle these.
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(app_url, &app_id),
            ManifestUpdateResult::kAppIsPlaceholder);
  histogram_tester_.ExpectBucketCount(
      kUpdateHistogramName, ManifestUpdateResult::kAppIsPlaceholder, 1);
}

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest,
                       CheckFindsThemeColorChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "theme_color": "$2"
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "blue"});
  AppId app_id = InstallWebApp();
  EXPECT_EQ(GetProvider().registrar().GetAppThemeColor(app_id), SK_ColorBLUE);

  // Check that OnWebAppInstalled and OnWebAppUninstalled are not called
  // if in-place web app update happens.
  WebAppInstallObserver install_observer(&GetProvider().registrar());
  install_observer.SetWebAppInstalledDelegate(
      base::BindLambdaForTesting([](const AppId& app_id) { NOTREACHED(); }));
  install_observer.SetWebAppUninstalledDelegate(
      base::BindLambdaForTesting([](const AppId& app_id) { NOTREACHED(); }));

  OverrideManifest(kManifestTemplate, {kInstallableIconList, "red"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(GetProvider().registrar().GetAppThemeColor(app_id), SK_ColorRED);
}

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest, CheckKeepsSameName) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "$1",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $2,
      "theme_color": "$3"
    }
  )";
  OverrideManifest(kManifestTemplate,
                   {"App name 1", kInstallableIconList, "blue"});
  AppId app_id = InstallWebApp();
  EXPECT_EQ(GetProvider().registrar().GetAppThemeColor(app_id), SK_ColorBLUE);
  EXPECT_EQ(GetProvider().registrar().GetAppShortName(app_id), "App name 1");

  OverrideManifest(kManifestTemplate,
                   {"App name 2", kInstallableIconList, "red"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(GetProvider().registrar().GetAppThemeColor(app_id), SK_ColorRED);
  // The app name must not change without user confirmation.
  EXPECT_EQ(GetProvider().registrar().GetAppShortName(app_id), "App name 1");
}

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest,
                       CheckFindsIconUrlChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {kAnotherInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
}

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest,
                       CheckUpdatedPolicyAppsNotUninstallable) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "theme_color": "$1",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"blue", kInstallableIconList});
  AppId app_id = InstallPolicyApp();
  EXPECT_FALSE(
      GetProvider().install_finalizer().CanUserUninstallFromSync(app_id));

  OverrideManifest(kManifestTemplate, {"red", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  // Policy installed apps should continue to be not uninstallable by the user
  // after updating.
  EXPECT_FALSE(
      GetProvider().install_finalizer().CanUserUninstallFromSync(app_id));
}

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest,
                       CheckFindsScopeChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "$1",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"/banners/", kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {"/", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(GetProvider().registrar().GetAppScope(app_id),
            http_server_.GetURL("/"));
}

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest,
                       CheckFindsDisplayChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "$1",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"minimal-ui", kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {"standalone", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(GetProvider().registrar().GetAppDisplayMode(app_id),
            DisplayMode::kStandalone);
}

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresDisplayBrowserChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "$1",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"standalone", kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {"browser", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppNotEligible);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppNotEligible, 1);
  EXPECT_EQ(GetProvider().registrar().GetAppDisplayMode(app_id),
            DisplayMode::kStandalone);
}

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest,
                       CheckFindsIconContentChange) {
  constexpr char kManifest[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": [
        {
          "src": "/web_apps/basic-192.png",
          "sizes": "192x192",
          "type": "image/png"
        }
      ]
    }
  )";
  OverrideManifest(kManifest, {});
  AppId app_id = InstallWebApp();

  // Replace the contents of basic-192.png with blue-192.png without changing
  // the URL.
  content::URLLoaderInterceptor url_interceptor(base::BindLambdaForTesting(
      [this](content::URLLoaderInterceptor::RequestParams* params)
          -> bool /*intercepted*/ {
        if (params->url_request.url ==
            http_server_.GetURL("/web_apps/basic-192.png")) {
          content::URLLoaderInterceptor::WriteResponse(
              "chrome/test/data/web_apps/blue-192.png", params->client.get());
          return true;
        }
        return false;
      }));

  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  // Check that the installed icon is now blue.
  base::RunLoop run_loop;
  GetProvider().icon_manager().ReadIcons(
      app_id, {192},
      base::BindLambdaForTesting(
          [&run_loop](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
            run_loop.Quit();
            EXPECT_EQ(icon_bitmaps.at(192).getColor(0, 0), SK_ColorBLUE);
          }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresIconDownloadFail) {
  constexpr char kManifest[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": [
        {
          "src": "/web_apps/basic-48.png",
          "sizes": "48x48",
          "type": "image/png"
        },
        {
          "src": "/web_apps/basic-192.png",
          "sizes": "192x192",
          "type": "image/png"
        }
      ]
    }
  )";
  OverrideManifest(kManifest, {});
  AppId app_id = InstallWebApp();

  // Make basic-48.png fail to download.
  // Replace the contents of basic-192.png with blue-192.png without changing
  // the URL.
  content::URLLoaderInterceptor url_interceptor(base::BindLambdaForTesting(
      [this](content::URLLoaderInterceptor::RequestParams* params)
          -> bool /*intercepted*/ {
        if (params->url_request.url ==
            http_server_.GetURL("/web_apps/basic-48.png")) {
          content::URLLoaderInterceptor::WriteResponse("malformed response", "",
                                                       params->client.get());
          return true;
        }
        if (params->url_request.url ==
            http_server_.GetURL("/web_apps/basic-192.png")) {
          content::URLLoaderInterceptor::WriteResponse(
              "chrome/test/data/web_apps/blue-192.png", params->client.get());
          return true;
        }
        return false;
      }));

  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kIconDownloadFailed);
  histogram_tester_.ExpectBucketCount(
      kUpdateHistogramName, ManifestUpdateResult::kIconDownloadFailed, 1);

  // Check that the installed icon is still black.
  base::RunLoop run_loop;
  GetProvider().icon_manager().ReadIcons(
      app_id, {48, 192},
      base::BindLambdaForTesting(
          [&run_loop](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
            run_loop.Quit();
            EXPECT_EQ(icon_bitmaps.at(48).getColor(0, 0), SK_ColorBLACK);
            EXPECT_EQ(icon_bitmaps.at(192).getColor(0, 0), SK_ColorBLACK);
          }));
  run_loop.Run();
}

class ManifestUpdateManagerSystemAppBrowserTest
    : public ManifestUpdateManagerBrowserTest {
 public:
  static constexpr char kSystemAppManifestText[] =
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
        "theme_color": "$1"
      })";

  ManifestUpdateManagerSystemAppBrowserTest()
      : system_app_(
            TestSystemWebAppInstallation::SetUpStandaloneSingleWindowApp()) {
    system_app_->SetManifest(base::ReplaceStringPlaceholders(
        kSystemAppManifestText, {"#0f0"}, nullptr));
  }

  void SetUpOnMainThread() override { system_app_->WaitForAppInstall(); }

 protected:
  std::unique_ptr<TestSystemWebAppInstallation> system_app_;
};

constexpr char
    ManifestUpdateManagerSystemAppBrowserTest::kSystemAppManifestText[];

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerSystemAppBrowserTest,
                       CheckUpdateSkipped) {
  system_app_->SetManifest(base::ReplaceStringPlaceholders(
      kSystemAppManifestText, {"#f00"}, nullptr));

  AppId app_id = system_app_->GetAppId();
  EXPECT_EQ(GetResultAfterPageLoad(system_app_->GetAppUrl(), &app_id),
            ManifestUpdateResult::kAppIsSystemWebApp);

  histogram_tester_.ExpectBucketCount(
      kUpdateHistogramName, ManifestUpdateResult::kAppIsSystemWebApp, 1);
  EXPECT_EQ(GetProvider().registrar().GetAppThemeColor(app_id), SK_ColorGREEN);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ManifestUpdateManagerBrowserTest,
                         ::testing::Values(ProviderType::kBookmarkApps,
                                           ProviderType::kWebApps),
                         ProviderTypeParamToString);

INSTANTIATE_TEST_SUITE_P(All,
                         ManifestUpdateManagerSystemAppBrowserTest,
                         ::testing::Values(ProviderType::kBookmarkApps,
                                           ProviderType::kWebApps),
                         ProviderTypeParamToString);

}  // namespace web_app
