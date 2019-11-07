// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_sync_bridge.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/test/test_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/test_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {

namespace {

bool IsSyncDataEqual(const WebApp& expected_app,
                     const syncer::EntityData& entity_data) {
  if (!entity_data.specifics.has_web_app())
    return false;

  const GURL sync_launch_url(entity_data.specifics.web_app().launch_url());
  if (expected_app.app_id() != GenerateAppIdFromURL(sync_launch_url))
    return false;

  auto app_from_sync = std::make_unique<WebApp>(expected_app.app_id());
  // ApplySyncDataToApp enforces kSync source on |app_from_sync|.
  ApplySyncDataToApp(entity_data.specifics.web_app(), app_from_sync.get());
  return expected_app == *app_from_sync;
}

bool SyncDataBatchMatchesRegistry(
    const Registry& registry,
    std::unique_ptr<syncer::DataBatch> data_batch) {
  if (!data_batch || !data_batch->HasNext())
    return false;

  syncer::KeyAndData key_and_data1 = data_batch->Next();
  if (!IsSyncDataEqual(*registry.at(key_and_data1.first),
                       *key_and_data1.second)) {
    return false;
  }

  if (!data_batch->HasNext())
    return false;

  syncer::KeyAndData key_and_data2 = data_batch->Next();
  if (!IsSyncDataEqual(*registry.at(key_and_data2.first),
                       *key_and_data2.second)) {
    return false;
  }

  return !data_batch->HasNext();
}

}  // namespace

class WebAppSyncBridgeTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();

    test_registry_controller_ =
        std::make_unique<TestWebAppRegistryController>();
    test_registry_controller_->SetUp(profile());
  }

  void TearDown() override {
    test_registry_controller_.reset();

    WebAppTest::TearDown();
  }

  void InitSyncBridge() { controller().Init(); }

  std::unique_ptr<WebApp> CreateWebApp(const std::string& url) {
    const GURL launch_url(url);
    const AppId app_id = GenerateAppIdFromURL(launch_url);

    auto web_app = std::make_unique<WebApp>(app_id);
    web_app->SetLaunchUrl(launch_url);
    web_app->SetUserDisplayMode(DisplayMode::kStandalone);
    web_app->SetName("Name");
    return web_app;
  }

  std::unique_ptr<WebApp> CreateWebAppWithSyncOnlyFields(
      const std::string& url) {
    const GURL launch_url(url);
    const AppId app_id = GenerateAppIdFromURL(launch_url);

    auto web_app = std::make_unique<WebApp>(app_id);
    web_app->AddSource(Source::kSync);
    web_app->SetLaunchUrl(launch_url);
    web_app->SetUserDisplayMode(DisplayMode::kStandalone);
    return web_app;
  }

  void InsertAppIntoRegistry(Registry* registry, std::unique_ptr<WebApp> app) {
    AppId app_id = app->app_id();
    ASSERT_FALSE(base::Contains(*registry, app_id));
    registry->emplace(std::move(app_id), std::move(app));
  }

  void InsertAppCopyIntoRegistry(Registry* registry, const WebApp& app) {
    registry->emplace(app.app_id(), std::make_unique<WebApp>(app));
  }

 protected:
  TestWebAppRegistryController& controller() {
    return *test_registry_controller_;
  }
  syncer::MockModelTypeChangeProcessor& processor() {
    return controller().processor();
  }
  TestWebAppDatabaseFactory& database_factory() {
    return controller().database_factory();
  }
  WebAppSyncBridge& sync_bridge() { return controller().sync_bridge(); }

 private:
  std::unique_ptr<TestWebAppRegistryController> test_registry_controller_;
};

TEST_F(WebAppSyncBridgeTest, GetData) {
  Registry registry;

  std::unique_ptr<WebApp> synced_app1 =
      CreateWebAppWithSyncOnlyFields("https://example.com/app1/");
  {
    WebApp::SyncData sync_data;
    sync_data.name = "Sync Name";
    sync_data.theme_color = SK_ColorCYAN;
    synced_app1->SetSyncData(std::move(sync_data));
  }
  InsertAppIntoRegistry(&registry, std::move(synced_app1));

  std::unique_ptr<WebApp> synced_app2 =
      CreateWebAppWithSyncOnlyFields("https://example.com/app2/");
  // sync_data is empty for this app.
  InsertAppIntoRegistry(&registry, std::move(synced_app2));

  std::unique_ptr<WebApp> policy_app = CreateWebApp("https://example.org/");
  policy_app->AddSource(Source::kPolicy);
  InsertAppIntoRegistry(&registry, std::move(policy_app));

  database_factory().WriteRegistry(registry);

  EXPECT_CALL(processor(), ModelReadyToSync(testing::_)).Times(1);
  InitSyncBridge();

  {
    WebAppSyncBridge::StorageKeyList storage_keys;
    storage_keys.push_back("unknown");
    for (const Registry::value_type& id_and_web_app : registry)
      storage_keys.push_back(id_and_web_app.first);

    base::RunLoop run_loop;
    sync_bridge().GetData(
        std::move(storage_keys),
        base::BindLambdaForTesting(
            [&](std::unique_ptr<syncer::DataBatch> data_batch) {
              EXPECT_TRUE(SyncDataBatchMatchesRegistry(registry,
                                                       std::move(data_batch)));
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    sync_bridge().GetAllDataForDebugging(base::BindLambdaForTesting(
        [&](std::unique_ptr<syncer::DataBatch> data_batch) {
          EXPECT_TRUE(
              SyncDataBatchMatchesRegistry(registry, std::move(data_batch)));
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

TEST_F(WebAppSyncBridgeTest, Identities) {
  std::unique_ptr<WebApp> app =
      CreateWebAppWithSyncOnlyFields("https://example.com/");
  std::unique_ptr<syncer::EntityData> entity_data = CreateSyncEntityData(*app);

  EXPECT_EQ(app->app_id(), sync_bridge().GetClientTag(*entity_data));
  EXPECT_EQ(app->app_id(), sync_bridge().GetStorageKey(*entity_data));
}

}  // namespace web_app
