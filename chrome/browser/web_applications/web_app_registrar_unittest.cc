// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_registrar.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind_helpers.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/test/test_web_app_sync_bridge.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "testing/gmock/include/gmock/gmock.h"
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
    web_app->SetLaunchUrl(GURL(url));
    web_app->SetLaunchContainer(LaunchContainer::kTab);

    registry.emplace(app_id, std::move(web_app));
  }

  return registry;
}

}  // namespace

class WebAppRegistrarTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();

    sync_bridge_ = std::make_unique<TestWebAppSyncBridge>();
    registrar_ =
        std::make_unique<WebAppRegistrar>(profile(), sync_bridge_.get());
  }

 protected:
  TestWebAppSyncBridge& sync_bridge() { return *sync_bridge_; }
  WebAppRegistrar& registrar() { return *registrar_; }

  std::set<AppId> RegisterAppsForTesting(Registry registry) {
    std::set<AppId> ids;

    for (auto& kv : registry) {
      ids.insert(kv.first);
      registrar().RegisterApp(std::move(kv.second));
    }

    return ids;
  }

  AppId InitRegistrarWithApp(std::unique_ptr<WebApp> app) {
    const AppId app_id = app->app_id();

    registrar().Init(base::DoNothing());
    DCHECK(registrar().is_empty());

    Registry registry;
    registry.emplace(app_id, std::move(app));

    sync_bridge().TakeOpenDatabaseCallback().Run(std::move(registry));
    return app_id;
  }

  std::set<AppId> InitRegistrarWithApps(const std::string& base_url,
                                        int num_apps) {
    std::set<AppId> app_ids;
    registrar().Init(base::DoNothing());
    DCHECK(registrar().is_empty());

    Registry registry = CreateRegistryForTesting(base_url, num_apps);
    for (auto& kv : registry)
      app_ids.insert(kv.second->app_id());

    sync_bridge().TakeOpenDatabaseCallback().Run(std::move(registry));
    return app_ids;
  }

  std::unique_ptr<WebApp> CreateWebApp(const std::string& url) {
    const GURL launch_url(url);
    const AppId app_id = GenerateAppIdFromURL(launch_url);

    auto web_app = std::make_unique<WebApp>(app_id);

    web_app->SetName("Name");
    web_app->SetLaunchUrl(launch_url);
    web_app->SetLaunchContainer(LaunchContainer::kWindow);
    return web_app;
  }

  void RegistrarCommitUpdate(std::unique_ptr<WebAppRegistryUpdate> update) {
    base::RunLoop run_loop;
    registrar().CommitUpdate(std::move(update),
                             base::BindLambdaForTesting([&](bool success) {
                               EXPECT_TRUE(success);
                               run_loop.Quit();
                             }));

    run_loop.Run();
  }

  void DestroySubsystems() {
    registrar_.reset();
    sync_bridge_.reset();
  }

 private:
  std::unique_ptr<TestWebAppSyncBridge> sync_bridge_;
  std::unique_ptr<WebAppRegistrar> registrar_;
};

TEST_F(WebAppRegistrarTest, CreateRegisterUnregister) {
  EXPECT_EQ(nullptr, registrar().GetAppById(AppId()));
  EXPECT_FALSE(registrar().GetAppById(AppId()));

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

  EXPECT_EQ(nullptr, registrar().GetAppById(app_id));
  EXPECT_EQ(nullptr, registrar().GetAppById(app_id2));
  EXPECT_TRUE(registrar().is_empty());

  registrar().RegisterApp(std::move(web_app));
  EXPECT_TRUE(registrar().IsInstalled(app_id));
  const WebApp* app = registrar().GetAppById(app_id);

  EXPECT_EQ(app_id, app->app_id());
  EXPECT_EQ(name, app->name());
  EXPECT_EQ(description, app->description());
  EXPECT_EQ(launch_url, app->launch_url());
  EXPECT_EQ(scope, app->scope());
  EXPECT_EQ(theme_color, app->theme_color());

  EXPECT_EQ(nullptr, registrar().GetAppById(app_id2));
  EXPECT_FALSE(registrar().is_empty());

  registrar().RegisterApp(std::move(web_app2));
  EXPECT_TRUE(registrar().IsInstalled(app_id2));
  const WebApp* app2 = registrar().GetAppById(app_id2);
  EXPECT_EQ(app_id2, app2->app_id());
  EXPECT_FALSE(registrar().is_empty());

  registrar().UnregisterApp(app_id);
  EXPECT_FALSE(registrar().IsInstalled(app_id));
  EXPECT_EQ(nullptr, registrar().GetAppById(app_id));
  EXPECT_FALSE(registrar().is_empty());

  // Check that app2 is still registered.
  app2 = registrar().GetAppById(app_id2);
  EXPECT_TRUE(registrar().IsInstalled(app_id2));
  EXPECT_EQ(app_id2, app2->app_id());

  registrar().UnregisterApp(app_id2);
  EXPECT_FALSE(registrar().IsInstalled(app_id2));
  EXPECT_EQ(nullptr, registrar().GetAppById(app_id2));
  EXPECT_TRUE(registrar().is_empty());
}

TEST_F(WebAppRegistrarTest, DestroyRegistrarOwningRegisteredApps) {
  const AppId app_id = GenerateAppIdFromURL(GURL("https://example.com/path"));
  const AppId app_id2 = GenerateAppIdFromURL(GURL("https://example.com/path2"));

  auto web_app = std::make_unique<WebApp>(app_id);
  registrar().RegisterApp(std::move(web_app));

  auto web_app2 = std::make_unique<WebApp>(app_id2);
  registrar().RegisterApp(std::move(web_app2));

  DestroySubsystems();
}

TEST_F(WebAppRegistrarTest, InitRegistrarAndDoForEachApp) {
  std::set<AppId> ids = InitRegistrarWithApps("https://example.com/path", 100);

  for (const WebApp& web_app : registrar().AllApps()) {
    const size_t num_removed = ids.erase(web_app.app_id());
    EXPECT_EQ(1U, num_removed);
  }

  EXPECT_TRUE(ids.empty());
}

TEST_F(WebAppRegistrarTest, AllAppsMutable) {
  std::set<AppId> ids = InitRegistrarWithApps("https://example.com/path", 10);

  for (WebApp& web_app : registrar().AllAppsMutable()) {
    web_app.SetLaunchContainer(LaunchContainer::kWindow);
    const size_t num_removed = ids.erase(web_app.app_id());
    EXPECT_EQ(1U, num_removed);
  }

  EXPECT_TRUE(ids.empty());
}

TEST_F(WebAppRegistrarTest, DoForEachAndUnregisterAllApps) {
  Registry registry = CreateRegistryForTesting("https://example.com/path", 100);
  auto ids = RegisterAppsForTesting(std::move(registry));
  EXPECT_EQ(100UL, ids.size());

  for (const WebApp& web_app : registrar().AllApps()) {
    const size_t num_removed = ids.erase(web_app.app_id());
    EXPECT_EQ(1U, num_removed);
  }
  EXPECT_TRUE(ids.empty());

  EXPECT_FALSE(registrar().is_empty());
  registrar().UnregisterAll();
  EXPECT_TRUE(registrar().is_empty());
}

TEST_F(WebAppRegistrarTest, AbstractWebAppSyncBridge) {
  std::set<AppId> ids = InitRegistrarWithApps("https://example.com/path", 100);

  // Add 1 app after Init.
  const AppId app_id = GenerateAppIdFromURL(GURL("https://example.com/path"));
  auto web_app = std::make_unique<WebApp>(app_id);
  registrar().RegisterApp(std::move(web_app));

  EXPECT_EQ(1UL, sync_bridge().write_web_app_ids().size());
  EXPECT_EQ(app_id, sync_bridge().write_web_app_ids()[0]);

  EXPECT_EQ(101UL, registrar().registry_for_testing().size());

  // Remove 1 app after Init.
  registrar().UnregisterApp(app_id);
  EXPECT_EQ(100UL, registrar().registry_for_testing().size());
  EXPECT_EQ(1UL, sync_bridge().delete_web_app_ids().size());
  EXPECT_EQ(app_id, sync_bridge().delete_web_app_ids()[0]);

  // Remove 100 apps after Init.
  registrar().UnregisterAll();
  for (auto& app_id : sync_bridge().delete_web_app_ids())
    ids.erase(app_id);

  EXPECT_TRUE(ids.empty());
  EXPECT_TRUE(registrar().is_empty());
}

TEST_F(WebAppRegistrarTest, GetAppDataFields) {
  const GURL launch_url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppIdFromURL(launch_url);
  const std::string name = "Name";
  const std::string description = "Description";
  const base::Optional<SkColor> theme_color = 0xAABBCCDD;
  const auto launch_container = LaunchContainer::kWindow;

  EXPECT_EQ(std::string(), registrar().GetAppShortName(app_id));
  EXPECT_EQ(GURL(), registrar().GetAppLaunchURL(app_id));

  auto web_app = std::make_unique<WebApp>(app_id);
  WebApp* web_app_ptr = web_app.get();

  web_app->SetName(name);
  web_app->SetDescription(description);
  web_app->SetThemeColor(theme_color);
  web_app->SetLaunchUrl(launch_url);
  web_app->SetLaunchContainer(launch_container);
  web_app->SetIsLocallyInstalled(/*is_locally_installed*/ false);

  registrar().RegisterApp(std::move(web_app));

  EXPECT_EQ(name, registrar().GetAppShortName(app_id));
  EXPECT_EQ(description, registrar().GetAppDescription(app_id));
  EXPECT_EQ(theme_color, registrar().GetAppThemeColor(app_id));
  EXPECT_EQ(launch_url, registrar().GetAppLaunchURL(app_id));
  EXPECT_EQ(launch_container, registrar().GetAppLaunchContainer(app_id));

  {
    EXPECT_FALSE(registrar().IsLocallyInstalled(app_id));

    EXPECT_FALSE(registrar().IsLocallyInstalled("unknown"));
    web_app_ptr->SetIsLocallyInstalled(/*is_locally_installed*/ true);
    EXPECT_TRUE(registrar().IsLocallyInstalled(app_id));
  }

  {
    EXPECT_EQ(LaunchContainer::kDefault,
              registrar().GetAppLaunchContainer("unknown"));

    web_app_ptr->SetLaunchContainer(LaunchContainer::kTab);
    EXPECT_EQ(LaunchContainer::kTab, registrar().GetAppLaunchContainer(app_id));

    registrar().SetAppLaunchContainer(app_id, LaunchContainer::kWindow);
    EXPECT_EQ(LaunchContainer::kWindow, web_app_ptr->launch_container());
  }
}

TEST_F(WebAppRegistrarTest, CanFindAppsInScope) {
  const GURL origin_scope("https://example.com/");
  const GURL app1_scope("https://example.com/app");
  const GURL app2_scope("https://example.com/app-two");
  const GURL app3_scope("https://not-example.com/app");

  std::vector<web_app::AppId> in_scope =
      registrar().FindAppsInScope(origin_scope);
  EXPECT_EQ(0u, in_scope.size());

  auto app1 = std::make_unique<WebApp>("1");
  app1->SetScope(app1_scope);
  registrar().RegisterApp(std::move(app1));

  in_scope = registrar().FindAppsInScope(origin_scope);
  EXPECT_THAT(in_scope, testing::UnorderedElementsAre("1"));

  in_scope = registrar().FindAppsInScope(app1_scope);
  EXPECT_THAT(in_scope, testing::UnorderedElementsAre("1"));

  auto app2 = std::make_unique<WebApp>("2");
  app2->SetScope(app2_scope);
  registrar().RegisterApp(std::move(app2));

  in_scope = registrar().FindAppsInScope(origin_scope);
  EXPECT_THAT(in_scope, testing::UnorderedElementsAre("1", "2"));

  in_scope = registrar().FindAppsInScope(app1_scope);
  EXPECT_THAT(in_scope, testing::UnorderedElementsAre("1", "2"));

  in_scope = registrar().FindAppsInScope(app2_scope);
  EXPECT_THAT(in_scope, testing::UnorderedElementsAre("2"));

  auto app3 = std::make_unique<WebApp>("3");
  app3->SetScope(app3_scope);
  registrar().RegisterApp(std::move(app3));

  in_scope = registrar().FindAppsInScope(origin_scope);
  EXPECT_THAT(in_scope, testing::UnorderedElementsAre("1", "2"));

  in_scope = registrar().FindAppsInScope(app3_scope);
  EXPECT_THAT(in_scope, testing::UnorderedElementsAre("3"));
}

TEST_F(WebAppRegistrarTest, CanFindAppWithUrlInScope) {
  const GURL origin_scope("https://example.com/");
  const GURL app1_scope("https://example.com/app");
  const GURL app2_scope("https://example.com/app-two");
  const GURL app3_scope("https://not-example.com/app");

  auto app1 = std::make_unique<WebApp>("1");
  app1->SetScope(app1_scope);
  registrar().RegisterApp(std::move(app1));

  base::Optional<AppId> app2_match =
      registrar().FindAppWithUrlInScope(app2_scope);
  DCHECK(app2_match);
  EXPECT_EQ(*app2_match, "1");

  base::Optional<AppId> app3_match =
      registrar().FindAppWithUrlInScope(app3_scope);
  EXPECT_FALSE(app3_match);

  auto app2 = std::make_unique<WebApp>("2");
  app2->SetScope(app2_scope);
  registrar().RegisterApp(std::move(app2));

  auto app3 = std::make_unique<WebApp>("3");
  app3->SetScope(app3_scope);
  registrar().RegisterApp(std::move(app3));

  base::Optional<AppId> origin_match =
      registrar().FindAppWithUrlInScope(origin_scope);
  EXPECT_FALSE(origin_match);

  base::Optional<AppId> app1_match =
      registrar().FindAppWithUrlInScope(app1_scope);
  DCHECK(app1_match);
  EXPECT_EQ(*app1_match, "1");

  app2_match = registrar().FindAppWithUrlInScope(app2_scope);
  DCHECK(app2_match);
  EXPECT_EQ(*app2_match, "2");

  app3_match = registrar().FindAppWithUrlInScope(app3_scope);
  DCHECK(app3_match);
  EXPECT_EQ(*app3_match, "3");
}

TEST_F(WebAppRegistrarTest, CanFindShortcutWithUrlInScope) {
  const GURL app1_page("https://example.com/app/page");
  const GURL app2_page("https://example.com/app-two/page");
  const GURL app3_page("https://not-example.com/app/page");

  const GURL app1_launch("https://example.com/app/launch");
  const GURL app2_launch("https://example.com/app-two/launch");
  const GURL app3_launch("https://not-example.com/app/launch");

  // Implicit scope "https://example.com/app/"
  auto app1 = std::make_unique<WebApp>("1");
  app1->SetLaunchUrl(app1_launch);
  registrar().RegisterApp(std::move(app1));

  base::Optional<AppId> app2_match =
      registrar().FindAppWithUrlInScope(app2_page);
  EXPECT_FALSE(app2_match);

  base::Optional<AppId> app3_match =
      registrar().FindAppWithUrlInScope(app3_page);
  EXPECT_FALSE(app3_match);

  auto app2 = std::make_unique<WebApp>("2");
  app2->SetLaunchUrl(app2_launch);
  registrar().RegisterApp(std::move(app2));

  auto app3 = std::make_unique<WebApp>("3");
  app3->SetLaunchUrl(app3_launch);
  registrar().RegisterApp(std::move(app3));

  base::Optional<AppId> app1_match =
      registrar().FindAppWithUrlInScope(app1_page);
  DCHECK(app1_match);
  EXPECT_EQ(app1_match, base::Optional<AppId>("1"));

  app2_match = registrar().FindAppWithUrlInScope(app2_page);
  DCHECK(app2_match);
  EXPECT_EQ(app2_match, base::Optional<AppId>("2"));

  app3_match = registrar().FindAppWithUrlInScope(app3_page);
  DCHECK(app3_match);
  EXPECT_EQ(app3_match, base::Optional<AppId>("3"));
}

TEST_F(WebAppRegistrarTest, FindPwaOverShortcut) {
  const GURL app1_launch("https://example.com/app/specific/launch1");

  const GURL app2_scope("https://example.com/app");
  const GURL app2_page("https://example.com/app/specific/page2");

  const GURL app3_launch("https://example.com/app/specific/launch3");

  auto app1 = std::make_unique<WebApp>("1");
  app1->SetLaunchUrl(app1_launch);
  registrar().RegisterApp(std::move(app1));

  auto app2 = std::make_unique<WebApp>("2");
  app2->SetScope(app2_scope);
  registrar().RegisterApp(std::move(app2));

  auto app3 = std::make_unique<WebApp>("3");
  app3->SetLaunchUrl(app3_launch);
  registrar().RegisterApp(std::move(app3));

  base::Optional<AppId> app2_match =
      registrar().FindAppWithUrlInScope(app2_page);
  DCHECK(app2_match);
  EXPECT_EQ(app2_match, base::Optional<AppId>("2"));
}

TEST_F(WebAppRegistrarTest, DatabaseWriteAndDeleteAppsFail) {
  auto app = CreateWebApp("https://example.com/path");
  auto app_id = app->app_id();

  sync_bridge().SetNextWriteWebAppsResult(false);
  registrar().RegisterApp(std::move(app));

  // nothing crashes, the production database impl would DLOG an error.
  EXPECT_FALSE(registrar().is_empty());

  sync_bridge().SetNextDeleteWebAppsResult(false);
  registrar().UnregisterApp(app_id);

  // nothing crashes, the production database impl would DLOG an error.
  EXPECT_TRUE(registrar().is_empty());
}

TEST_F(WebAppRegistrarTest, BeginAndCommitUpdate) {
  std::set<AppId> ids = InitRegistrarWithApps("https://example.com/path", 10);

  std::unique_ptr<WebAppRegistryUpdate> update = registrar().BeginUpdate();

  for (auto& app_id : ids) {
    WebApp* app = update->UpdateApp(app_id);
    EXPECT_TRUE(app);
    app->SetName("New Name");
  }

  // Acquire each app second time to make sure update requests get merged.
  for (auto& app_id : ids) {
    WebApp* app = update->UpdateApp(app_id);
    EXPECT_TRUE(app);
    app->SetLaunchContainer(LaunchContainer::kWindow);
  }

  RegistrarCommitUpdate(std::move(update));

  // Make sure that all app ids were written to the database.
  EXPECT_EQ(ids.size(), sync_bridge().write_web_app_ids().size());

  for (auto& written_app_id : sync_bridge().write_web_app_ids())
    ids.erase(written_app_id);

  EXPECT_TRUE(ids.empty());
}

TEST_F(WebAppRegistrarTest, CommitEmptyUpdate) {
  std::set<AppId> ids = InitRegistrarWithApps("https://example.com/path", 10);

  {
    std::unique_ptr<WebAppRegistryUpdate> update = registrar().BeginUpdate();
    RegistrarCommitUpdate(std::move(update));

    EXPECT_TRUE(sync_bridge().write_web_app_ids().empty());
  }

  {
    std::unique_ptr<WebAppRegistryUpdate> update = registrar().BeginUpdate();
    update.reset();
    RegistrarCommitUpdate(std::move(update));

    EXPECT_TRUE(sync_bridge().write_web_app_ids().empty());
  }

  {
    std::unique_ptr<WebAppRegistryUpdate> update = registrar().BeginUpdate();

    WebApp* app = update->UpdateApp("unknown");
    EXPECT_FALSE(app);

    RegistrarCommitUpdate(std::move(update));

    EXPECT_TRUE(sync_bridge().write_web_app_ids().empty());
  }
}

TEST_F(WebAppRegistrarTest, CommitUpdateFailed) {
  auto app = CreateWebApp("https://example.com/path");
  auto app_id = InitRegistrarWithApp(std::move(app));
  EXPECT_TRUE(sync_bridge().write_web_app_ids().empty());

  std::unique_ptr<WebAppRegistryUpdate> update = registrar().BeginUpdate();

  WebApp* update_app = update->UpdateApp(app_id);
  EXPECT_TRUE(update_app);
  update_app->SetName("New Name");

  sync_bridge().SetNextWriteWebAppsResult(false);

  base::RunLoop run_loop;
  registrar().CommitUpdate(std::move(update),
                           base::BindLambdaForTesting([&](bool success) {
                             EXPECT_FALSE(success);
                             run_loop.Quit();
                           }));

  run_loop.Run();
}

TEST_F(WebAppRegistrarTest, ScopedRegistryUpdate) {
  std::set<AppId> ids = InitRegistrarWithApps("https://example.com/path", 10);

  // Test empty update first.
  { ScopedRegistryUpdate update(&registrar()); }
  EXPECT_TRUE(sync_bridge().write_web_app_ids().empty());

  {
    ScopedRegistryUpdate update(&registrar());

    for (auto& app_id : ids) {
      WebApp* app = update->UpdateApp(app_id);
      EXPECT_TRUE(app);
      app->SetDescription("New Description");
    }
  }

  // Make sure that all app ids were written to the database.
  EXPECT_EQ(ids.size(), sync_bridge().write_web_app_ids().size());

  for (auto& written_app_id : sync_bridge().write_web_app_ids())
    ids.erase(written_app_id);

  EXPECT_TRUE(ids.empty());
}

}  // namespace web_app
