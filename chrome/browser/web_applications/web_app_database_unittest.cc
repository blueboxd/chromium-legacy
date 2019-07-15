// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_database.h"

#include <memory>
#include <tuple>

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/test/test_web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/sync/model/model_type_store.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

bool operator==(const WebApp::IconInfo& icon_info,
                const WebApp::IconInfo& icon_info2) {
  return std::tie(icon_info.url, icon_info.size_in_px) ==
         std::tie(icon_info2.url, icon_info2.size_in_px);
}

bool operator==(const WebApp& web_app, const WebApp& web_app2) {
  return std::tie(web_app.app_id(), web_app.name(), web_app.launch_url(),
                  web_app.description(), web_app.scope(), web_app.theme_color(),
                  web_app.icons()) ==
         std::tie(web_app2.app_id(), web_app2.name(), web_app2.launch_url(),
                  web_app2.description(), web_app2.scope(),
                  web_app2.theme_color(), web_app2.icons());
}

bool operator!=(const WebApp& web_app, const WebApp& web_app2) {
  return !operator==(web_app, web_app2);
}

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
    database_ = std::make_unique<WebAppDatabase>(database_factory_.get());
    registrar_ = std::make_unique<WebAppRegistrar>(nullptr, database_.get());
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

    app->SetName(name);
    app->SetDescription(description);
    app->SetLaunchUrl(GURL(launch_url));
    app->SetScope(GURL(scope));
    app->SetThemeColor(theme_color);

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

    database_->ReadRegistry(base::BindLambdaForTesting([&](Registry r) {
      registry = std::move(r);
      run_loop.Quit();
    }));

    run_loop.Run();
    return registry;
  }

  bool IsDatabaseRegistryEqualToRegistrar() {
    Registry registry = ReadRegistry();
    return IsRegistryEqual(registrar_->registry(), registry);
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
  // Must be created before TestWebAppDatabaseFactory.
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<TestWebAppDatabaseFactory> database_factory_;
  std::unique_ptr<WebAppDatabase> database_;
  std::unique_ptr<WebAppRegistrar> registrar_;
};

TEST_F(WebAppDatabaseTest, WriteAndReadRegistry) {
  InitRegistrar();
  EXPECT_TRUE(registrar_->is_empty());

  const int num_apps = 100;
  const std::string base_url = "https://example.com/path";

  auto app = CreateWebApp(base_url, 0);
  auto app_id = app->app_id();
  registrar_->RegisterApp(std::move(app));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());

  for (int i = 1; i <= num_apps; ++i) {
    auto extra_app = CreateWebApp(base_url, i);
    registrar_->RegisterApp(std::move(extra_app));
  }
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());

  registrar_->UnregisterApp(app_id);
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());

  registrar_->UnregisterAll();
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppDatabaseTest, OpenDatabaseAndReadRegistry) {
  Registry registry = WriteWebApps("https://example.com/path", 100);

  InitRegistrar();
  EXPECT_TRUE(IsRegistryEqual(registrar_->registry(), registry));
}

TEST_F(WebAppDatabaseTest, WebAppWithoutOptionalFields) {
  InitRegistrar();

  const auto launch_url = GURL("https://example.com/");
  const AppId app_id = GenerateAppIdFromURL(GURL(launch_url));
  const std::string name = "Name";

  auto app = std::make_unique<WebApp>(app_id);
  // Required fields:
  app->SetLaunchUrl(launch_url);
  app->SetName(name);
  // Let optional fields be empty:
  EXPECT_TRUE(app->description().empty());
  EXPECT_TRUE(app->scope().is_empty());
  EXPECT_FALSE(app->theme_color().has_value());
  EXPECT_TRUE(app->icons().empty());
  registrar_->RegisterApp(std::move(app));

  Registry registry = ReadRegistry();
  EXPECT_EQ(1UL, registry.size());

  std::unique_ptr<WebApp>& app_copy = registry.at(app_id);

  // Required fields were serialized:
  EXPECT_EQ(app_id, app_copy->app_id());
  EXPECT_EQ(launch_url, app_copy->launch_url());
  EXPECT_EQ(name, app_copy->name());
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

  registrar_->RegisterApp(std::move(app));

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
