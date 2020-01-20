// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/web_applications/test/test_system_web_app_installation.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "content/public/common/url_constants.h"

namespace web_app {

namespace {

// Assumes url is like "chrome://web-app/index.html". Returns "web-app";
// This function is needed because at the time TestSystemWebInstallation is
// initialized, chrome scheme is not yet registered with GURL, so it will be
// parsed as PathURL, resulting in an empty host.
std::string GetDataSourceNameFromSystemAppInstallUrl(const GURL& url) {
  DCHECK(url.SchemeIs(content::kChromeUIScheme));

  const std::string& spec = url.spec();
  size_t p = strlen(content::kChromeUIScheme);

  DCHECK_EQ("://", spec.substr(p, 3));
  p += 3;

  size_t pos_after_host = spec.find("/", p);
  DCHECK(pos_after_host != std::string::npos);

  return spec.substr(p, pos_after_host - p);
}
}  // namespace

TestSystemWebAppInstallation::TestSystemWebAppInstallation(SystemAppType type,
                                                           SystemAppInfo info)
    : type_(type),
      web_ui_controller_factory_(
          GetDataSourceNameFromSystemAppInstallUrl(info.install_url)) {
  test_web_app_provider_creator_ = std::make_unique<TestWebAppProviderCreator>(
      base::BindOnce(&TestSystemWebAppInstallation::CreateWebAppProvider,
                     // base::Unretained is safe here. This callback is called
                     // at TestingProfile::Init, which is at test startup.
                     // TestSystemWebAppInstallation is intended to have the
                     // same lifecycle as the test, it won't be destroyed before
                     // the test finishes.
                     base::Unretained(this), info));
  content::WebUIControllerFactory::RegisterFactory(&web_ui_controller_factory_);
}

TestSystemWebAppInstallation::~TestSystemWebAppInstallation() {
  content::WebUIControllerFactory::UnregisterFactoryForTesting(
      &web_ui_controller_factory_);
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpTabbedMultiWindowApp() {
  SystemAppInfo terminal_system_app_info(
      "Terminal", GURL("chrome://test-system-app/pwa.html"));
  terminal_system_app_info.single_window = false;
  return base::WrapUnique(new TestSystemWebAppInstallation(
      SystemAppType::TERMINAL, terminal_system_app_info));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpStandaloneSingleWindowApp() {
  return base::WrapUnique(new TestSystemWebAppInstallation(
      SystemAppType::SETTINGS,
      SystemAppInfo("OSSettings", GURL("chrome://test-system-app/pwa.html"))));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppThatReceivesLaunchDirectory() {
  SystemAppInfo media_system_app_info(
      "Media", GURL("chrome://test-system-app/pwa.html"));
  media_system_app_info.include_launch_directory = true;
  return base::WrapUnique(new TestSystemWebAppInstallation(
      SystemAppType::MEDIA, media_system_app_info));
}

std::unique_ptr<KeyedService>
TestSystemWebAppInstallation::CreateWebAppProvider(SystemAppInfo info,
                                                   Profile* profile) {
  profile_ = profile;
  auto provider = std::make_unique<TestWebAppProvider>(profile);
  auto system_web_app_manager = std::make_unique<SystemWebAppManager>(profile);
  system_web_app_manager->SetSystemAppsForTesting({{type_, info}});
  provider->SetSystemWebAppManager(std::move(system_web_app_manager));
  provider->Start();
  return provider;
}

void TestSystemWebAppInstallation::WaitForAppInstall() {
  base::RunLoop run_loop;
  WebAppProvider::Get(profile_)
      ->system_web_app_manager()
      .on_apps_synchronized()
      .Post(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

AppId TestSystemWebAppInstallation::GetAppId() {
  return WebAppProvider::Get(profile_)
      ->system_web_app_manager()
      .GetAppIdForSystemApp(type_)
      .value();
}

const GURL& TestSystemWebAppInstallation::GetAppUrl() {
  return WebAppProvider::Get(profile_)->registrar().GetAppLaunchURL(GetAppId());
}

SystemAppType TestSystemWebAppInstallation::GetType() {
  return type_;
}

}  // namespace web_app
