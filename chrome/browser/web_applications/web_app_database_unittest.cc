// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_database.h"

#include <memory>

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/test/test_web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/sync/model/model_type_store.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {

bool IsRegistryEqual(const Registry& registry, const Registry& registry2) {
  if (registry.size() != registry2.size())
    return false;

  for (auto& kv : registry) {
    const WebApp* web_app = kv.second.get();
    const WebApp* web_app2 = registry2.at(web_app->app_id()).get();
    if (*web_app != *web_app2)
      return false;
  }

  return true;
}

}  // namespace

class WebAppDatabaseTest : public testing::Test {
 public:
  WebAppDatabaseTest() {
    database_factory_ = std::make_unique<TestWebAppDatabaseFactory>();
    sync_bridge_ = std::make_unique<WebAppSyncBridge>(database_factory_.get());
    registrar_ = std::make_unique<WebAppRegistrar>(nullptr, sync_bridge_.get());
  }

  void InitRegistrar() {
    base::RunLoop run_loop;

    registrar_->Init(base::BindLambdaForTesting([&]() { run_loop.Quit(); }));

    run_loop.Run();
  }

  static std::unique_ptr<WebApp> CreateWebApp(const std::string& base_url,
                                              int suffix) {
    const auto launch_url = base_url + base::NumberToString(suffix);
    const AppId app_id = GenerateAppIdFromURL(GURL(launch_url));
    const std::string name = "Name" + base::NumberToString(suffix);
    const std::string description =
        "Description" + base::NumberToString(suffix);
    const std::string scope =
        base_url + "/scope" + base::NumberToString(suffix);
    const base::Optional<SkColor> theme_color = suffix;

    auto app = std::make_unique<WebApp>(app_id);

    // Generate all possible permutations of field values in a random way:
    if (suffix & 1)
      app->AddSource(Source::kSystem);
    if (suffix & 2)
      app->AddSource(Source::kPolicy);
    if (suffix & 4)
      app->AddSource(Source::kWebAppStore);
    if (suffix & 8)
      app->AddSource(Source::kSync);
    if (suffix & 16)
      app->AddSource(Source::kDefault);
    if ((suffix & 31) == 0)
      app->AddSource(Source::kSync);

    app->SetName(name);
    app->SetDescription(description);
    app->SetLaunchUrl(GURL(launch_url));
    app->SetScope(GURL(scope));
    app->SetThemeColor(theme_color);
    app->SetIsLocallyInstalled(!(suffix & 2));
    app->SetLaunchContainer((suffix & 1) ? LaunchContainer::kTab
                                         : LaunchContainer::kWindow);

    const std::string icon_url =
        base_url + "/icon" + base::NumberToString(suffix);
    const int icon_size_in_px = 256;

    WebApp::Icons icons;
    icons.push_back({GURL(icon_url), icon_size_in_px});

    app->SetIcons(std::move(icons));

    return app;
  }

  Registry ReadRegistry() {
    Registry registry;
    base::RunLoop run_loop;

    sync_bridge().ReadRegistry(base::BindLambdaForTesting([&](Registry r) {
      registry = std::move(r);
      run_loop.Quit();
    }));

    run_loop.Run();
    return registry;
  }

  bool IsDatabaseRegistryEqualToRegistrar() {
    Registry registry = ReadRegistry();
    return IsRegistryEqual(registrar_->registry_for_testing(), registry);
  }

  void WriteBatch(
      std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch) {
    base::RunLoop run_loop;

    database_factory_->store()->CommitWriteBatch(
        std::move(write_batch),
        base::BindLambdaForTesting(
            [&](const base::Optional<syncer::ModelError>& error) {
              EXPECT_FALSE(error);
              run_loop.Quit();
            }));

    run_loop.Run();
  }

  Registry WriteWebApps(const std::string& base_url, int num_apps) {
    Registry registry;

    auto write_batch = database_factory_->store()->CreateWriteBatch();

    for (int i = 0; i < num_apps; ++i) {
      auto app = CreateWebApp(base_url, i);
      auto proto = WebAppDatabase::CreateWebAppProto(*app);
      const auto app_id = app->app_id();

      write_batch->WriteData(app_id, proto->SerializeAsString());

      registry.emplace(app_id, std::move(app));
    }

    WriteBatch(std::move(write_batch));

    return registry;
  }

 protected:
  WebAppSyncBridge& sync_bridge() { return *sync_bridge_; }
  WebAppRegistrar& registrar() { return *registrar_; }

 private:
  // Must be created before TestWebAppDatabaseFactory.
  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<TestWebAppDatabaseFactory> database_factory_;
  std::unique_ptr<WebAppSyncBridge> sync_bridge_;
  std::unique_ptr<WebAppRegistrar> registrar_;
};

TEST_F(WebAppDatabaseTest, WriteAndReadRegistry) {
  InitRegistrar();
  EXPECT_TRUE(registrar().is_empty());

  const int num_apps = 100;
  const std::string base_url = "https://example.com/path";

  auto app = CreateWebApp(base_url, 0);
  auto app_id = app->app_id();
  registrar().RegisterApp(std::move(app));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());

  for (int i = 1; i <= num_apps; ++i) {
    auto extra_app = CreateWebApp(base_url, i);
    registrar().RegisterApp(std::move(extra_app));
  }
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());

  registrar().UnregisterApp(app_id);
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());

  registrar().UnregisterAll();
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppDatabaseTest, WriteAndDeleteAppsWithCallbacks) {
  InitRegistrar();
  EXPECT_TRUE(registrar().is_empty());

  const int num_apps = 10;
  const std::string base_url = "https://example.com/path";

  Registry registry;
  WebAppDatabase::AppsToWrite apps_to_write;
  std::vector<AppId> apps_to_delete;

  for (int i = 0; i < num_apps; ++i) {
    auto app = CreateWebApp(base_url, i);
    apps_to_write.insert(app.get());
    apps_to_delete.push_back(app->app_id());
    registry.emplace(app->app_id(), std::move(app));
  }

  {
    base::RunLoop run_loop;
    sync_bridge().WriteWebApps(std::move(apps_to_write),
                               base::BindLambdaForTesting([&](bool success) {
                                 EXPECT_TRUE(success);
                                 run_loop.Quit();
                               }));
    run_loop.Run();

    Registry registry_written = ReadRegistry();
    EXPECT_TRUE(IsRegistryEqual(registry_written, registry));
  }

  {
    base::RunLoop run_loop;
    sync_bridge().DeleteWebApps(std::move(apps_to_delete),
                                base::BindLambdaForTesting([&](bool success) {
                                  EXPECT_TRUE(success);
                                  run_loop.Quit();
                                }));
    run_loop.Run();

    Registry registry_deleted = ReadRegistry();
    EXPECT_TRUE(registry_deleted.empty());
  }
}

TEST_F(WebAppDatabaseTest, OpenDatabaseAndReadRegistry) {
  Registry registry = WriteWebApps("https://example.com/path", 100);

  InitRegistrar();
  EXPECT_TRUE(IsRegistryEqual(registrar().registry_for_testing(), registry));
}

TEST_F(WebAppDatabaseTest, WebAppWithoutOptionalFields) {
  InitRegistrar();

  const auto launch_url = GURL("https://example.com/");
  const AppId app_id = GenerateAppIdFromURL(GURL(launch_url));
  const std::string name = "Name";
  const auto launch_container = LaunchContainer::kTab;

  auto app = std::make_unique<WebApp>(app_id);

  // Required fields:
  app->SetLaunchUrl(launch_url);
  app->SetName(name);
  app->SetLaunchContainer(launch_container);
  app->SetIsLocallyInstalled(false);

  EXPECT_FALSE(app->HasAnySources());
  for (int i = Source::kMinValue; i < Source::kMaxValue; ++i) {
    app->AddSource(static_cast<Source::Type>(i));
    EXPECT_TRUE(app->HasAnySources());
  }

  // Let optional fields be empty:
  EXPECT_TRUE(app->description().empty());
  EXPECT_TRUE(app->scope().is_empty());
  EXPECT_FALSE(app->theme_color().has_value());
  EXPECT_TRUE(app->icons().empty());
  registrar().RegisterApp(std::move(app));

  Registry registry = ReadRegistry();
  EXPECT_EQ(1UL, registry.size());

  std::unique_ptr<WebApp>& app_copy = registry.at(app_id);

  // Required fields were serialized:
  EXPECT_EQ(app_id, app_copy->app_id());
  EXPECT_EQ(launch_url, app_copy->launch_url());
  EXPECT_EQ(name, app_copy->name());
  EXPECT_EQ(launch_container, app_copy->launch_container());
  EXPECT_FALSE(app_copy->is_locally_installed());

  for (int i = Source::kMinValue; i < Source::kMaxValue; ++i) {
    EXPECT_TRUE(app_copy->HasAnySources());
    app_copy->RemoveSource(static_cast<Source::Type>(i));
  }
  EXPECT_FALSE(app_copy->HasAnySources());

  // No optional fields.
  EXPECT_TRUE(app_copy->description().empty());
  EXPECT_TRUE(app_copy->scope().is_empty());
  EXPECT_FALSE(app_copy->theme_color().has_value());
  EXPECT_TRUE(app_copy->icons().empty());
}

TEST_F(WebAppDatabaseTest, WebAppWithManyIcons) {
  InitRegistrar();

  const int num_icons = 32;
  const std::string base_url = "https://example.com/path";

  auto app = CreateWebApp(base_url, 0);
  auto app_id = app->app_id();

  WebApp::Icons icons;
  for (int i = 1; i <= num_icons; ++i) {
    const std::string icon_url =
        base_url + "/icon" + base::NumberToString(num_icons);
    // Let size equals the icon's number squared.
    const int icon_size_in_px = i * i;
    icons.push_back({GURL(icon_url), icon_size_in_px});
  }
  app->SetIcons(std::move(icons));

  registrar().RegisterApp(std::move(app));

  Registry registry = ReadRegistry();
  EXPECT_EQ(1UL, registry.size());

  std::unique_ptr<WebApp>& app_copy = registry.at(app_id);
  EXPECT_EQ(static_cast<unsigned>(num_icons), app_copy->icons().size());
  for (int i = 1; i <= num_icons; ++i) {
    const int icon_size_in_px = i * i;
    EXPECT_EQ(icon_size_in_px, app_copy->icons()[i - 1].size_in_px);
  }
}

}  // namespace web_app
