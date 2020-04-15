// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/test/test_system_web_app_installation.h"
#include "chrome/browser/web_applications/test/test_system_web_app_url_data_source.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/url_constants.h"

namespace web_app {

namespace {

enum class WebUIType {
  // kChrome WebUIs registration works by creating a WebUIControllerFactory
  // which then register a URLDataSource to serve resources.
  kChrome,
  // kChromeUntrusted WebUIs don't have WebUIControllers and their
  // URLDataSources need to be registered directly.
  kChromeUntrusted,
};

WebUIType GetWebUIType(const GURL& url) {
  if (url.SchemeIs(content::kChromeUIScheme))
    return WebUIType::kChrome;
  if (url.SchemeIs(content::kChromeUIUntrustedScheme))
    return WebUIType::kChromeUntrusted;
  NOTREACHED();
  return WebUIType::kChrome;
}

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

// Returns the scheme and host from an install URL e.g. for
// chrome-untrusted://web-app/index.html this returns
// chrome-untrusted://web-app/.
std::string GetChromeUntrustedDataSourceNameFromInstallUrl(const GURL& url) {
  DCHECK(url.SchemeIs(content::kChromeUIUntrustedScheme));

  const std::string& spec = url.spec();
  size_t p = strlen(content::kChromeUIUntrustedScheme);
  DCHECK_EQ("://", spec.substr(p, 3));
  p += 3;

  size_t pos_after_host = spec.find("/", p);
  DCHECK(pos_after_host != std::string::npos);

  // The Data Source name must include "/" after the host.
  ++pos_after_host;
  return spec.substr(0, pos_after_host);
}

}  // namespace

TestSystemWebAppInstallation::TestSystemWebAppInstallation(SystemAppType type,
                                                           SystemAppInfo info)
    : type_(type) {
  if (GetWebUIType(info.install_url) == WebUIType::kChrome) {
    web_ui_controller_factory_ =
        std::make_unique<TestSystemWebAppWebUIControllerFactory>(
            GetDataSourceNameFromSystemAppInstallUrl(info.install_url));
    content::WebUIControllerFactory::RegisterFactory(
        web_ui_controller_factory_.get());
  }

  test_web_app_provider_creator_ = std::make_unique<TestWebAppProviderCreator>(
      base::BindOnce(&TestSystemWebAppInstallation::CreateWebAppProvider,
                     // base::Unretained is safe here. This callback is called
                     // at TestingProfile::Init, which is at test startup.
                     // TestSystemWebAppInstallation is intended to have the
                     // same lifecycle as the test, it won't be destroyed before
                     // the test finishes.
                     base::Unretained(this), info));
}

TestSystemWebAppInstallation::~TestSystemWebAppInstallation() {
  if (web_ui_controller_factory_.get()) {
    content::WebUIControllerFactory::UnregisterFactoryForTesting(
        web_ui_controller_factory_.get());
  }
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
TestSystemWebAppInstallation::SetUpAppThatReceivesLaunchFiles(
    IncludeLaunchDirectory include_launch_directory) {
  SystemAppInfo media_system_app_info(
      "Media", GURL("chrome://test-system-app/pwa.html"));

  if (include_launch_directory == IncludeLaunchDirectory::kYes)
    media_system_app_info.include_launch_directory = true;
  else
    media_system_app_info.include_launch_directory = false;

  return base::WrapUnique(new TestSystemWebAppInstallation(
      SystemAppType::MEDIA, media_system_app_info));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppWithEnabledOriginTrials(
    const OriginTrialsMap& origin_to_trials) {
  SystemAppInfo media_system_app_info(
      "Media", GURL("chrome://test-system-app/pwa.html"));
  media_system_app_info.enabled_origin_trials = origin_to_trials;
  return base::WrapUnique(new TestSystemWebAppInstallation(
      SystemAppType::MEDIA, media_system_app_info));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppNotShownInLauncher() {
  SystemAppInfo app_info("Test", GURL("chrome://test-system-app/pwa.html"));
  app_info.show_in_launcher = false;
  return base::WrapUnique(new TestSystemWebAppInstallation(
      SystemAppType::SETTINGS, std::move(app_info)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppNotShownInSearch() {
  SystemAppInfo app_info("Test", GURL("chrome://test-system-app/pwa.html"));
  app_info.show_in_search = false;
  return base::WrapUnique(new TestSystemWebAppInstallation(
      SystemAppType::SETTINGS, std::move(app_info)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppWithAdditionalSearchTerms() {
  SystemAppInfo app_info("Test", GURL("chrome://test-system-app/pwa.html"));
  app_info.additional_search_terms = {IDS_SETTINGS_SECURITY};
  return base::WrapUnique(new TestSystemWebAppInstallation(
      SystemAppType::SETTINGS, std::move(app_info)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpChromeUntrustedApp() {
  return base::WrapUnique(new TestSystemWebAppInstallation(
      SystemAppType::SETTINGS,
      SystemAppInfo("Test",
                    GURL("chrome-untrusted://test-system-app/pwa.html"))));
}

std::unique_ptr<KeyedService>
TestSystemWebAppInstallation::CreateWebAppProvider(SystemAppInfo info,
                                                   Profile* profile) {
  profile_ = profile;
  if (GetWebUIType(info.install_url) == WebUIType::kChromeUntrusted) {
    web_app::AddTestURLDataSource(
        GetChromeUntrustedDataSourceNameFromInstallUrl(info.install_url),
        profile);
  }

  auto provider = std::make_unique<TestWebAppProvider>(profile);
  auto system_web_app_manager = std::make_unique<SystemWebAppManager>(profile);
  system_web_app_manager->SetSystemAppsForTesting({{type_, info}});
  system_web_app_manager->SetUpdatePolicyForTesting(update_policy_);
  provider->SetSystemWebAppManager(std::move(system_web_app_manager));
  provider->Start();
  return provider;
}

void TestSystemWebAppInstallation::WaitForAppInstall() {
  base::RunLoop run_loop;
  WebAppProvider::Get(profile_)
      ->system_web_app_manager()
      .on_apps_synchronized()
      .Post(FROM_HERE, base::BindLambdaForTesting([&]() {
              // Wait one execution loop for on_apps_synchronized() to be
              // called on all listeners.
              base::ThreadTaskRunnerHandle::Get()->PostTask(
                  FROM_HERE, run_loop.QuitClosure());
            }));
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

void TestSystemWebAppInstallation::SetManifest(std::string manifest) {
  web_ui_controller_factory_->set_manifest(std::move(manifest));
}

}  // namespace web_app
