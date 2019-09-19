// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"

#include "base/test/bind_test_util.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "net/dns/mock_host_resolver.h"

namespace web_app {

WebAppControllerBrowserTestBase::WebAppControllerBrowserTestBase() {
  if (GetParam() == ControllerType::kUnifiedControllerWithWebApp) {
    scoped_feature_list_.InitWithFeatures(
        {features::kDesktopPWAsWithoutExtensions}, {});
  } else if (GetParam() == ControllerType::kUnifiedControllerWithBookmarkApp) {
    scoped_feature_list_.InitWithFeatures(
        {features::kDesktopPWAsUnifiedUiController},
        {features::kDesktopPWAsWithoutExtensions});
  } else {
    scoped_feature_list_.InitWithFeatures(
        {}, {features::kDesktopPWAsUnifiedUiController,
             features::kDesktopPWAsWithoutExtensions});
  }
}

WebAppControllerBrowserTestBase::~WebAppControllerBrowserTestBase() = default;

WebAppControllerBrowserTest::WebAppControllerBrowserTest()
    : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
  scoped_feature_list_.InitWithFeatures(
      {}, {predictors::kSpeculativePreconnectFeature});
}

WebAppControllerBrowserTest::~WebAppControllerBrowserTest() = default;

void WebAppControllerBrowserTest::SetUp() {
  https_server_.AddDefaultHandlers(GetChromeTestDataDir());

  extensions::ExtensionBrowserTest::SetUp();
}

AppId WebAppControllerBrowserTest::InstallPWA(const GURL& app_url) {
  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->app_url = app_url;
  web_app_info->scope = app_url.GetWithoutFilename();
  web_app_info->open_as_window = true;
  return InstallWebApp(std::move(web_app_info));
}

AppId WebAppControllerBrowserTest::InstallWebApp(
    std::unique_ptr<WebApplicationInfo>&& web_app_info) {
  AppId app_id;
  base::RunLoop run_loop;
  auto* provider = web_app::WebAppProvider::Get(profile());
  DCHECK(provider);
  provider->install_manager().InstallWebAppFromInfo(
      std::move(web_app_info), ForInstallableSite::kYes,
      WebappInstallSource::OMNIBOX_INSTALL_ICON,
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, web_app::InstallResultCode code) {
            EXPECT_EQ(web_app::InstallResultCode::kSuccessNewInstall, code);
            app_id = installed_app_id;
            run_loop.Quit();
          }));

  run_loop.Run();
  return app_id;
}

content::WebContents* WebAppControllerBrowserTest::OpenApplication(
    const AppId& app_id) {
  auto* provider = web_app::WebAppProvider::Get(profile());
  DCHECK(provider);
  ui_test_utils::UrlLoadObserver url_observer(
      provider->registrar().GetAppLaunchURL(app_id),
      content::NotificationService::AllSources());

  AppLaunchParams params(profile(), app_id,
                         apps::mojom::LaunchContainer::kLaunchContainerWindow,
                         WindowOpenDisposition::NEW_WINDOW,
                         apps::mojom::AppLaunchSource::kSourceTest);
  content::WebContents* contents =
      apps::LaunchService::Get(profile())->OpenApplication(params);
  url_observer.Wait();
  return contents;
}

void WebAppControllerBrowserTest::SetUpInProcessBrowserTestFixture() {
  extensions::ExtensionBrowserTest::SetUpInProcessBrowserTestFixture();
  cert_verifier_.SetUpInProcessBrowserTestFixture();
}

void WebAppControllerBrowserTest::TearDownInProcessBrowserTestFixture() {
  extensions::ExtensionBrowserTest::TearDownInProcessBrowserTestFixture();
  cert_verifier_.TearDownInProcessBrowserTestFixture();
}

void WebAppControllerBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);
  cert_verifier_.SetUpCommandLine(command_line);
}

void WebAppControllerBrowserTest::SetUpOnMainThread() {
  extensions::ExtensionBrowserTest::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");

  // By default, all SSL cert checks are valid. Can be overridden in tests.
  cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
}

}  // namespace web_app
