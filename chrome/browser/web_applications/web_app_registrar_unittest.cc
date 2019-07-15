// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_registrar.h"

#include <set>
#include <string>
#include <utility>

#include "base/bind_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/test/test_web_app_database.h"
#include "chrome/browser/web_applications/web_app.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {

Registry CreateRegistryForTesting(const std::string& base_url, int num_apps) {
  Registry registry;

  for (int i = 0; i < num_apps; ++i) {
    const auto url = base_url + base::NumberToString(i);
    const AppId app_id = GenerateAppIdFromURL(GURL(url));
    auto web_app = std::make_unique<WebApp>(app_id);
    registry.emplace(app_id, std::move(web_app));
  }

  return registry;
}

std::set<AppId> RegisterAppsForTesting(WebAppRegistrar* registrar,
                                       Registry&& registry) {
  std::set<AppId> ids;

  for (auto& kv : registry) {
    ids.insert(kv.first);
    registrar->RegisterApp(std::move(kv.second));
  }

  return ids;
}

}  // namespace

TEST(WebAppRegistrar, CreateRegisterUnregister) {
  auto database = std::make_unique<TestWebAppDatabase>();
  auto registrar = std::make_unique<WebAppRegistrar>(nullptr, database.get());

  EXPECT_EQ(nullptr, registrar->GetAppById(AppId()));
  EXPECT_FALSE(registrar->GetAppById(AppId()));

  const GURL launch_url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppIdFromURL(launch_url);
  const std::string name = "Name";
  const std::string description = "Description";
  const GURL scope = GURL("https://example.com/scope");
  const base::Optional<SkColor> theme_color = 0xAABBCCDD;

  const GURL launch_url2 = GURL("https://example.com/path2");
  const AppId app_id2 = GenerateAppIdFromURL(launch_url2);

  auto web_app = std::make_unique<WebApp>(app_id);
  auto web_app2 = std::make_unique<WebApp>(app_id2);

  web_app->SetName(name);
  web_app->SetDescription(description);
  web_app->SetLaunchUrl(launch_url);
  web_app->SetScope(scope);
  web_app->SetThemeColor(theme_color);

  EXPECT_EQ(nullptr, registrar->GetAppById(app_id));
  EXPECT_EQ(nullptr, registrar->GetAppById(app_id2));
  EXPECT_TRUE(registrar->is_empty());

  registrar->RegisterApp(std::move(web_app));
  EXPECT_TRUE(registrar->IsInstalled(app_id));
  WebApp* app = registrar->GetAppById(app_id);

  EXPECT_EQ(app_id, app->app_id());
  EXPECT_EQ(name, app->name());
  EXPECT_EQ(description, app->description());
  EXPECT_EQ(launch_url, app->launch_url());
  EXPECT_EQ(scope, app->scope());
  EXPECT_EQ(theme_color, app->theme_color());

  EXPECT_EQ(nullptr, registrar->GetAppById(app_id2));
  EXPECT_FALSE(registrar->is_empty());

  registrar->RegisterApp(std::move(web_app2));
  EXPECT_TRUE(registrar->IsInstalled(app_id2));
  WebApp* app2 = registrar->GetAppById(app_id2);
  EXPECT_EQ(app_id2, app2->app_id());
  EXPECT_FALSE(registrar->is_empty());

  registrar->UnregisterApp(app_id);
  EXPECT_FALSE(registrar->IsInstalled(app_id));
  EXPECT_EQ(nullptr, registrar->GetAppById(app_id));
  EXPECT_FALSE(registrar->is_empty());

  // Check that app2 is still registered.
  app2 = registrar->GetAppById(app_id2);
  EXPECT_TRUE(registrar->IsInstalled(app_id2));
  EXPECT_EQ(app_id2, app2->app_id());

  registrar->UnregisterApp(app_id2);
  EXPECT_FALSE(registrar->IsInstalled(app_id2));
  EXPECT_EQ(nullptr, registrar->GetAppById(app_id2));
  EXPECT_TRUE(registrar->is_empty());
}

TEST(WebAppRegistrar, DestroyRegistrarOwningRegisteredApps) {
  auto database = std::make_unique<TestWebAppDatabase>();
  auto registrar = std::make_unique<WebAppRegistrar>(nullptr, database.get());

  const AppId app_id = GenerateAppIdFromURL(GURL("https://example.com/path"));
  const AppId app_id2 = GenerateAppIdFromURL(GURL("https://example.com/path2"));

  auto web_app = std::make_unique<WebApp>(app_id);
  registrar->RegisterApp(std::move(web_app));

  auto web_app2 = std::make_unique<WebApp>(app_id2);
  registrar->RegisterApp(std::move(web_app2));

  registrar.reset();
}

TEST(WebAppRegistrar, ForEachAndUnregisterAll) {
  auto database = std::make_unique<TestWebAppDatabase>();
  auto registrar = std::make_unique<WebAppRegistrar>(nullptr, database.get());

  Registry registry = CreateRegistryForTesting("https://example.com/path", 100);
  auto ids = RegisterAppsForTesting(registrar.get(), std::move(registry));
  EXPECT_EQ(100UL, ids.size());

  for (auto& kv : registrar->registry()) {
    const WebApp* web_app = kv.second.get();
    const size_t num_removed = ids.erase(web_app->app_id());
    EXPECT_EQ(1U, num_removed);
  }
  EXPECT_TRUE(ids.empty());

  EXPECT_FALSE(registrar->is_empty());
  registrar->UnregisterAll();
  EXPECT_TRUE(registrar->is_empty());
}

TEST(WebAppRegistrar, AbstractWebAppDatabase) {
  auto database = std::make_unique<TestWebAppDatabase>();
  auto registrar = std::make_unique<WebAppRegistrar>(nullptr, database.get());

  registrar->Init(base::DoNothing());
  EXPECT_TRUE(registrar->is_empty());

  // Load 100 apps.
  Registry registry = CreateRegistryForTesting("https://example.com/path", 100);
  // Copy their ids.
  std::set<AppId> ids;
  for (auto& kv : registry)
    ids.insert(kv.first);
  EXPECT_EQ(100UL, ids.size());

  database->TakeOpenDatabaseCallback().Run(std::move(registry));

  // Add 1 app after opening.
  const AppId app_id = GenerateAppIdFromURL(GURL("https://example.com/path"));
  auto web_app = std::make_unique<WebApp>(app_id);
  registrar->RegisterApp(std::move(web_app));
  EXPECT_EQ(app_id, database->write_web_app_id());
  EXPECT_EQ(101UL, registrar->registry().size());

  // Remove 1 app after opening.
  registrar->UnregisterApp(app_id);
  EXPECT_EQ(100UL, registrar->registry().size());
  EXPECT_EQ(1UL, database->delete_web_app_ids().size());
  EXPECT_EQ(app_id, database->delete_web_app_ids()[0]);

  // Remove 100 apps after opening.
  registrar->UnregisterAll();
  for (auto& app_id : database->delete_web_app_ids())
    ids.erase(app_id);

  EXPECT_TRUE(ids.empty());
  EXPECT_TRUE(registrar->is_empty());
}

TEST(WebAppRegistrar, GetAppDetails) {
  auto database = std::make_unique<TestWebAppDatabase>();
  auto registrar = std::make_unique<WebAppRegistrar>(nullptr, database.get());

  const GURL launch_url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppIdFromURL(launch_url);
  const std::string name = "Name";
  const std::string description = "Description";
  const base::Optional<SkColor> theme_color = 0xAABBCCDD;

  EXPECT_EQ(std::string(), registrar->GetAppShortName(app_id));
  EXPECT_EQ(GURL(), registrar->GetAppLaunchURL(app_id));

  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->SetName(name);
  web_app->SetDescription(description);
  web_app->SetThemeColor(theme_color);
  web_app->SetLaunchUrl(launch_url);
  registrar->RegisterApp(std::move(web_app));

  EXPECT_EQ(name, registrar->GetAppShortName(app_id));
  EXPECT_EQ(description, registrar->GetAppDescription(app_id));
  EXPECT_EQ(theme_color, registrar->GetAppThemeColor(app_id));
  EXPECT_EQ(launch_url, registrar->GetAppLaunchURL(app_id));
}

}  // namespace web_app
