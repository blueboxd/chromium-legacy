// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/app_service_wrapper.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/common/chrome_features.h"
#include "chrome/services/app_service/public/cpp/app_update.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/mojom/app.mojom.h"
#include "components/arc/mojom/app_permissions.mojom.h"
#include "components/arc/test/fake_app_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace app_time {

namespace {

constexpr char kArcPackage1[] = "com.example.app1";
constexpr char kArcApp1[] = "ArcApp1";
constexpr char kArcPackage2[] = "com.example.app2";
constexpr char kArcApp2[] = "ArcApp2";

arc::mojom::ArcPackageInfoPtr CreateArcAppPackage(
    const std::string& package_name) {
  auto package = arc::mojom::ArcPackageInfo::New();
  package->package_name = package_name;
  package->package_version = 1;
  package->last_backup_android_id = 1;
  package->last_backup_time = 1;
  package->sync = false;
  package->system = false;
  package->permissions = base::flat_map<::arc::mojom::AppPermission, bool>();
  return package;
}

arc::mojom::AppInfo CreateArcAppInfo(const std::string& package_name,
                                     const std::string& name) {
  arc::mojom::AppInfo app;
  app.package_name = package_name;
  app.name = name;
  app.activity = base::StrCat({name, "Activity"});
  app.sticky = true;
  return app;
}

}  // namespace

class AppServiceWrapperTest : public testing::Test {
 public:
  class MockListener : public AppServiceWrapper::EventListener {
   public:
    MockListener() = default;
    MockListener(const MockListener&) = delete;
    MockListener& operator=(const MockListener&) = delete;
    ~MockListener() override = default;

    MOCK_METHOD1(OnAppInstalled, void(const AppId& app_id));
    MOCK_METHOD1(OnAppUninstalled, void(const AppId& app_id));
  };

 protected:
  AppServiceWrapperTest() = default;
  AppServiceWrapperTest(const AppServiceWrapperTest&) = delete;
  AppServiceWrapperTest& operator=(const AppServiceWrapperTest&) = delete;
  ~AppServiceWrapperTest() override = default;

  ArcAppTest& arc_test() { return arc_test_; }
  AppServiceWrapper& tested_wrapper() { return tested_wrapper_; }
  MockListener& test_listener() { return test_listener_; }

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();

    feature_list_.InitAndEnableFeature(features::kPerAppTimeLimits);
    app_service_test_.SetUp(&profile_);
    arc_test_.SetUp(&profile_);
    tested_wrapper_.AddObserver(&test_listener_);
  }

  void TearDown() override {
    tested_wrapper_.RemoveObserver(&test_listener_);
    arc_test_.TearDown();

    testing::Test::TearDown();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void SimulateAppInstalled(const AppId& app_id, const std::string& app_name) {
    const std::string& package_name = app_id.app_id();
    arc_test_.AddPackage(CreateArcAppPackage(package_name)->Clone());

    const arc::mojom::AppInfo app = CreateArcAppInfo(package_name, app_name);
    arc_test_.app_instance()->SendPackageAppListRefreshed(package_name, {app});

    task_environment_.RunUntilIdle();
  }

  void SimulateAppUninstalled(const AppId& app_id) {
    const std::string& package_name = app_id.app_id();
    arc_test_.app_instance()->UninstallPackage(package_name);

    task_environment_.RunUntilIdle();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;

  TestingProfile profile_;
  apps::AppServiceTest app_service_test_;
  ArcAppTest arc_test_;

  AppServiceWrapper tested_wrapper_{&profile_};
  MockListener test_listener_;
};

// Tests GetInstalledApps() method.
TEST_F(AppServiceWrapperTest, GetInstalledApps) {
  // No ARC apps installed.
  EXPECT_EQ(0u, tested_wrapper().GetInstalledApps().size());

  // Update the list of ARC apps.
  const std::vector<arc::mojom::AppInfo>& expected_apps =
      arc_test().fake_apps();
  arc_test().app_instance()->SendRefreshAppList(expected_apps);
  RunUntilIdle();

  const std::vector<AppId> installed_apps = tested_wrapper().GetInstalledApps();
  ASSERT_EQ(expected_apps.size(), installed_apps.size());
  for (const auto& app : expected_apps) {
    EXPECT_TRUE(base::Contains(
        installed_apps, AppId(apps::mojom::AppType::kArc, app.package_name)));
  }
}

// Tests installs and uninstalls of Arc apps.
TEST_F(AppServiceWrapperTest, ArcAppInstallation) {
  // No app installed.
  EXPECT_EQ(0u, tested_wrapper().GetInstalledApps().size());

  // Install first ARC app.
  const AppId app1(apps::mojom::AppType::kArc, kArcPackage1);
  EXPECT_CALL(test_listener(), OnAppInstalled(app1)).Times(1);
  SimulateAppInstalled(app1, kArcApp1);

  std::vector<AppId> installed_apps = tested_wrapper().GetInstalledApps();
  ASSERT_EQ(1u, installed_apps.size());
  EXPECT_EQ(app1, installed_apps[0]);

  // Install second ARC app.
  const AppId app2(apps::mojom::AppType::kArc, kArcPackage2);
  EXPECT_CALL(test_listener(), OnAppInstalled(app2)).Times(1);
  SimulateAppInstalled(app2, kArcApp2);

  installed_apps = tested_wrapper().GetInstalledApps();
  EXPECT_EQ(2u, installed_apps.size());

  // Uninstall first ARC app.
  EXPECT_CALL(test_listener(), OnAppUninstalled(app1)).Times(1);
  SimulateAppUninstalled(app1);

  installed_apps = tested_wrapper().GetInstalledApps();
  ASSERT_EQ(1u, installed_apps.size());
  EXPECT_EQ(app2, installed_apps[0]);
}

}  // namespace app_time
}  // namespace chromeos
