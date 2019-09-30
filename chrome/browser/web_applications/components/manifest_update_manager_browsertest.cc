// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/manifest_update_manager.h"

#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_app_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {

namespace {

const char* kInstallableIconList = R"(
  [
    {
      "src": "launcher-icon-4x.png",
      "sizes": "192x192",
      "type": "image/png"
    }
  ]
)";

const char* kAnotherInstallableIconList = R"(
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
    GetManifestUpdateManager(browser_).SetResultCallbackForTesting(base::Bind(
        &UpdateCheckResultAwaiter::OnResult, base::Unretained(this)));
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

class ManifestUpdateManagerBrowserTest : public InProcessBrowserTest {
 public:
  ManifestUpdateManagerBrowserTest() = default;
  ~ManifestUpdateManagerBrowserTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kDesktopPWAsLocalUpdating);

    http_server_.AddDefaultHandlers(GetChromeTestDataDir());
    http_server_.RegisterRequestHandler(base::BindRepeating(
        &ManifestUpdateManagerBrowserTest::RequestHandlerOverride,
        base::Unretained(this)));
    ASSERT_TRUE(http_server_.Start());

    InProcessBrowserTest::SetUp();
  }

  std::unique_ptr<net::test_server::HttpResponse> RequestHandlerOverride(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL() != override_url_)
      return nullptr;
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_FOUND);
    http_response->set_content(override_content_);
    return std::move(http_response);
  }

  void OverrideManifest(const char* manifest_template,
                        const std::vector<std::string>& substitutions) {
    override_url_ = GetManifestURL();
    override_content_ = base::ReplaceStringPlaceholders(manifest_template,
                                                        substitutions, nullptr);
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

    auto* provider = WebAppProviderBase::GetProviderBase(browser()->profile());

    AppId app_id;
    base::RunLoop run_loop;
    InstallManager::InstallParams params;
    params.add_to_applications_menu = false;
    params.add_to_desktop = false;
    params.add_to_quick_launch_bar = false;
    params.bypass_service_worker_check = true;
    params.require_manifest = false;
    provider->install_manager().InstallWebAppWithParams(
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  net::EmbeddedTestServer http_server_;
  GURL override_url_;
  std::string override_content_;

  DISALLOW_COPY_AND_ASSIGN(ManifestUpdateManagerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckOutOfScopeNavigation) {
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), nullptr),
            ManifestUpdateResult::kNoAppInScope);

  AppId app_id = InstallWebApp();

  EXPECT_EQ(GetResultAfterPageLoad(GURL("http://example.org"), nullptr),
            ManifestUpdateResult::kNoAppInScope);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest, CheckIsThrottled) {
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
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckCancelledByWebContentsDestroyed) {
  AppId app_id = InstallWebApp();
  GetManifestUpdateManager(browser()).hang_update_checks_for_testing();

  GURL url = GetAppURL();
  UpdateCheckResultAwaiter awaiter(browser(), url);
  ui_test_utils::NavigateToURL(browser(), url);
  chrome::CloseTab(browser());
  EXPECT_EQ(std::move(awaiter).AwaitNextResult(),
            ManifestUpdateResult::kWebContentsDestroyed);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckCancelledByAppUninstalled) {
  AppId app_id = InstallWebApp();
  GetManifestUpdateManager(browser()).hang_update_checks_for_testing();

  GURL url = GetAppURL();
  UpdateCheckResultAwaiter awaiter(browser(), url);
  ui_test_utils::NavigateToURL(browser(), url);
  WebAppProviderBase::GetProviderBase(browser()->profile())
      ->install_finalizer()
      .UninstallWebApp(app_id, base::DoNothing());
  EXPECT_EQ(std::move(awaiter).AwaitNextResult(),
            ManifestUpdateResult::kAppUninstalled);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresWhitespaceDifferences) {
  const char* manifest_template = R"(
    {
      "name": "Test app",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1
      $2
    }
  )";
  OverrideManifest(manifest_template, {kInstallableIconList, ""});
  AppId app_id = InstallWebApp();

  OverrideManifest(manifest_template, {kInstallableIconList, "\n\n\n\n"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpToDate);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresNameChange) {
  const char* manifest_template = R"(
    {
      "name": "$1",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(manifest_template, {"Test app name", kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(manifest_template,
                   {"Different app name", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpToDate);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresShortNameChange) {
  const char* manifest_template = R"(
    {
      "name": "Test app name",
      "short_name": "$1",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(manifest_template,
                   {"Short test app name", kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(manifest_template,
                   {"Different short test app name", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpToDate);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresStartUrlChange) {
  const char* manifest_template = R"(
    {
      "name": "Test app name",
      "start_url": "$1",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(manifest_template, {"a.html", kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(manifest_template, {"b.html", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpToDate);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresNoManifestChange) {
  const char* manifest_template = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1
    }
  )";
  OverrideManifest(manifest_template, {kInstallableIconList});
  AppId app_id = InstallWebApp();
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpToDate);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresInvalidManifest) {
  const char* manifest_template = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      $2
    }
  )";
  OverrideManifest(manifest_template, {kInstallableIconList, ""});
  AppId app_id = InstallWebApp();
  OverrideManifest(manifest_template, {kInstallableIconList,
                                       "invalid manifest syntax !@#$%^*&()"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppDataInvalid);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsThemeColorChange) {
  const char* manifest_template = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "theme_color": "$2"
    }
  )";
  OverrideManifest(manifest_template, {kInstallableIconList, "blue"});
  AppId app_id = InstallWebApp();

  OverrideManifest(manifest_template, {kInstallableIconList, "red"});
  // TODO(crbug.com/926083): Implement successful updating.
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpdateFailed);
}

// TODO(crbug.com/926083): Implement icon URL change detection.
IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       DISABLED_CheckFindsIconUrlChange) {
  const char* manifest_template = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1
    }
  )";
  OverrideManifest(manifest_template, {kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(manifest_template, {kAnotherInstallableIconList});
  // TODO(crbug.com/926083): Implement successful updating.
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpdateFailed);
}

}  // namespace web_app
