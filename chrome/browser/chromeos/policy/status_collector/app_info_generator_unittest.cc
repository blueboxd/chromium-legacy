// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/status_collector/app_info_generator.h"

#include <memory>

#include "base/test/bind_test_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/web_applications/test/test_app_registrar.h"
#include "chrome/browser/web_applications/test/test_install_finalizer.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
#include "chrome/browser/web_applications/test/test_web_app_registry_controller.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Property;

namespace em = enterprise_management;

namespace policy {

auto EqApp(const std::string& app_id,
           const std::string& name,
           const em::AppInfo::Status status,
           const std::string& version,
           const em::AppInfo::AppType app_type) {
  return AllOf(Property(&em::AppInfo::app_id, app_id),
               Property(&em::AppInfo::app_name, name),
               Property(&em::AppInfo::status, status),
               Property(&em::AppInfo::version, version),
               Property(&em::AppInfo::app_type, app_type));
}

class AppInfoGeneratorTest : public ::testing::Test {
 protected:
  static apps::mojom::AppPtr MakeApp(const std::string& app_id,
                                     const std::string& name,
                                     const apps::mojom::Readiness readiness,
                                     const std::string& version,
                                     const apps::mojom::AppType app_type) {
    auto app = apps::mojom::App::New();
    app->app_id = app_id;
    app->name = name;
    app->readiness = readiness;
    app->version = version;
    app->app_type = app_type;
    return app;
  }

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();

    web_app::WebAppProviderFactory::GetInstance()->SetTestingFactoryAndUse(
        profile_.get(),
        base::BindLambdaForTesting([this](content::BrowserContext* context)
                                       -> std::unique_ptr<KeyedService> {
          Profile* profile = Profile::FromBrowserContext(context);
          auto provider =
              std::make_unique<web_app::TestWebAppProvider>(profile);
          auto app_registrar = std::make_unique<web_app::TestAppRegistrar>();

          app_registrar_ = app_registrar.get();
          provider->SetRegistrar(std::move(app_registrar));
          provider->Start();
          return provider;
        }));
  }

  apps::AppRegistryCache& GetCache() {
    apps::AppServiceProxy* proxy =
        apps::AppServiceProxyFactory::GetForProfile(profile_.get());
    return proxy->AppRegistryCache();
  }

  std::unique_ptr<AppInfoGenerator> GetGenerator() {
    return std::make_unique<AppInfoGenerator>(profile_.get());
  }

  web_app::TestAppRegistrar& web_app_registrar() { return *app_registrar_; }
  Profile& profile() { return *profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  web_app::TestAppRegistrar* app_registrar_;
};

TEST_F(AppInfoGeneratorTest, GenerateInventoryList) {
  std::vector<apps::mojom::AppPtr> deltas;
  deltas.push_back(MakeApp("a", "FirstApp",
                           apps::mojom::Readiness::kDisabledByPolicy, "1.1",
                           apps::mojom::AppType::kArc));
  deltas.push_back(MakeApp("b", "SecondApp", apps::mojom::Readiness::kReady,
                           "1.2", apps::mojom::AppType::kExtension));
  deltas.push_back(MakeApp("c", "ThirdApp",
                           apps::mojom::Readiness::kUninstalledByUser, "",
                           apps::mojom::AppType::kCrostini));
  GetCache().OnApps(std::move(deltas));

  auto app_infos = GetGenerator()->Generate();

  EXPECT_THAT(
      app_infos,
      ElementsAre(EqApp("a", "FirstApp", em::AppInfo_Status_STATUS_DISABLED,
                        "1.1", em::AppInfo_AppType_TYPE_ARC),
                  EqApp("b", "SecondApp", em::AppInfo_Status_STATUS_INSTALLED,
                        "1.2", em::AppInfo_AppType_TYPE_EXTENSION),
                  EqApp("c", "ThirdApp", em::AppInfo_Status_STATUS_UNINSTALLED,
                        "", em::AppInfo_AppType_TYPE_CROSTINI)));
}

TEST_F(AppInfoGeneratorTest, GenerateWebApp) {
  std::vector<apps::mojom::AppPtr> deltas;
  deltas.push_back(MakeApp("c", "App",
                           apps::mojom::Readiness::kUninstalledByUser, "",
                           apps::mojom::AppType::kWeb));
  GetCache().OnApps(std::move(deltas));

  web_app::TestAppRegistrar::AppInfo app = {
      GURL::EmptyGURL(), web_app::ExternalInstallSource::kExternalDefault,
      GURL("http://app.com/app")};
  web_app_registrar().AddExternalApp("c", app);

  auto app_infos = GetGenerator()->Generate();

  EXPECT_THAT(app_infos,
              ElementsAre(EqApp("http://app.com/", "http://app.com/",
                                em::AppInfo_Status_STATUS_UNINSTALLED, "",
                                em::AppInfo_AppType_TYPE_WEB)));
}

}  // namespace policy
