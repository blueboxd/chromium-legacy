// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_sync_bridge.h"

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
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

using testing::_;

using AppsList = std::vector<std::unique_ptr<WebApp>>;

void RemoveWebAppFromAppsList(AppsList* apps_list, const AppId& app_id) {
  base::EraseIf(*apps_list, [app_id](const std::unique_ptr<WebApp>& app) {
    return app->app_id() == app_id;
  });
}

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

std::unique_ptr<WebApp> CreateWebApp(const std::string& url) {
  const GURL launch_url(url);
  const AppId app_id = GenerateAppIdFromURL(launch_url);

  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->SetLaunchUrl(launch_url);
  web_app->SetUserDisplayMode(DisplayMode::kStandalone);
  web_app->SetName("Name");
  return web_app;
}

std::unique_ptr<WebApp> CreateWebAppWithSyncOnlyFields(const std::string& url) {
  const GURL launch_url(url);
  const AppId app_id = GenerateAppIdFromURL(launch_url);

  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->AddSource(Source::kSync);
  web_app->SetLaunchUrl(launch_url);
  web_app->SetUserDisplayMode(DisplayMode::kStandalone);
  return web_app;
}

AppsList CreateAppsList(const std::string& base_url, int num_apps) {
  AppsList apps_list;

  for (int i = 0; i < num_apps; ++i) {
    apps_list.push_back(
        CreateWebAppWithSyncOnlyFields(base_url + base::NumberToString(i)));
  }

  return apps_list;
}

void InsertAppIntoRegistry(Registry* registry, std::unique_ptr<WebApp> app) {
  AppId app_id = app->app_id();
  ASSERT_FALSE(base::Contains(*registry, app_id));
  registry->emplace(std::move(app_id), std::move(app));
}

void InsertAppsListIntoRegistry(Registry* registry, const AppsList& apps_list) {
  for (const std::unique_ptr<WebApp>& app : apps_list)
    registry->emplace(app->app_id(), std::make_unique<WebApp>(*app));
}

void ConvertAppsListToEntityChangeList(
    const AppsList& apps_list,
    syncer::EntityChangeList* sync_data_list) {
  for (auto& app : apps_list) {
    std::unique_ptr<syncer::EntityChange> entity_change =
        syncer::EntityChange::CreateAdd(app->app_id(),
                                        CreateSyncEntityData(*app));
    sync_data_list->push_back(std::move(entity_change));
  }
}

// Returns true if the app converted from entity_data exists in the apps_list.
bool RemoveEntityDataAppFromAppsList(const std::string& storage_key,
                                     const syncer::EntityData& entity_data,
                                     AppsList* apps_list) {
  for (auto& app : *apps_list) {
    if (app->app_id() == storage_key) {
      if (!IsSyncDataEqual(*app, entity_data))
        return false;

      RemoveWebAppFromAppsList(apps_list, storage_key);
      return true;
    }
  }

  return false;
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

  bool IsDatabaseRegistryEqualToRegistrar() {
    Registry registry = database_factory().ReadRegistry();
    return IsRegistryEqual(registrar_registry(), registry);
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
  WebAppRegistrar& registrar() { return controller().mutable_registrar(); }
  Registry& registrar_registry() {
    return controller().mutable_registrar().registry();
  }

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

  EXPECT_CALL(processor(), ModelReadyToSync(_)).Times(1);
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

TEST_F(WebAppSyncBridgeTest, MergeSyncData_LocalSetAndServerSetAreEmpty) {
  InitSyncBridge();

  syncer::EntityChangeList sync_data_list;

  EXPECT_CALL(processor(), Put(_, _, _)).Times(0);

  sync_bridge().MergeSyncData(sync_bridge().CreateMetadataChangeList(),
                              std::move(sync_data_list));
}

TEST_F(WebAppSyncBridgeTest, MergeSyncData_LocalSetEqualsServerSet) {
  AppsList apps = CreateAppsList("https://example.com/", 10);

  Registry registry;
  InsertAppsListIntoRegistry(&registry, apps);
  database_factory().WriteRegistry(registry);
  InitSyncBridge();

  // The incoming list of apps from the sync server.
  syncer::EntityChangeList sync_data_list;
  ConvertAppsListToEntityChangeList(apps, &sync_data_list);

  EXPECT_CALL(processor(), Put(_, _, _)).Times(0);

  sync_bridge().MergeSyncData(sync_bridge().CreateMetadataChangeList(),
                              std::move(sync_data_list));

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppSyncBridgeTest, MergeSyncData_LocalSetGreaterThanServerSet) {
  AppsList local_and_server_apps = CreateAppsList("https://example.com/", 10);
  AppsList expected_local_apps_to_upload =
      CreateAppsList("https://example.org/", 10);

  Registry registry;
  InsertAppsListIntoRegistry(&registry, local_and_server_apps);
  InsertAppsListIntoRegistry(&registry, expected_local_apps_to_upload);
  database_factory().WriteRegistry(registry);
  InitSyncBridge();

  auto metadata_change_list = sync_bridge().CreateMetadataChangeList();
  syncer::MetadataChangeList* metadata_ptr = metadata_change_list.get();

  syncer::EntityChangeList sync_data_list;
  ConvertAppsListToEntityChangeList(local_and_server_apps, &sync_data_list);

  base::RunLoop run_loop;
  ON_CALL(processor(), Put(_, _, _))
      .WillByDefault([&](const std::string& storage_key,
                         std::unique_ptr<syncer::EntityData> entity_data,
                         syncer::MetadataChangeList* metadata) {
        EXPECT_EQ(metadata_ptr, metadata);
        EXPECT_TRUE(RemoveEntityDataAppFromAppsList(
            storage_key, *entity_data, &expected_local_apps_to_upload));
        if (expected_local_apps_to_upload.empty())
          run_loop.Quit();
      });

  sync_bridge().MergeSyncData(std::move(metadata_change_list),
                              std::move(sync_data_list));
  run_loop.Run();

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppSyncBridgeTest, MergeSyncData_LocalSetLessThanServerSet) {
  AppsList local_and_server_apps = CreateAppsList("https://example.com/", 10);
  AppsList expected_apps_to_install =
      CreateAppsList("https://example.org/", 10);
  // These fields are not synced, these are just expected values.
  for (std::unique_ptr<WebApp>& expected_app_to_install :
       expected_apps_to_install) {
    expected_app_to_install->SetIsLocallyInstalled(
        AreAppsLocallyInstalledByDefault());
    expected_app_to_install->SetIsInSyncInstall(true);
  }

  Registry registry;
  InsertAppsListIntoRegistry(&registry, local_and_server_apps);
  database_factory().WriteRegistry(registry);
  InitSyncBridge();

  syncer::EntityChangeList sync_data_list;
  ConvertAppsListToEntityChangeList(expected_apps_to_install, &sync_data_list);
  ConvertAppsListToEntityChangeList(local_and_server_apps, &sync_data_list);

  EXPECT_CALL(processor(), Put(_, _, _)).Times(0);

  base::RunLoop run_loop;
  controller().SetInstallWebAppsAfterSyncDelegate(base::BindLambdaForTesting(
      [&](std::vector<WebApp*> apps_to_install,
          TestWebAppRegistryController::RepeatingInstallCallback callback) {
        for (WebApp* app_to_install : apps_to_install) {
          // The app must be registered.
          EXPECT_TRUE(registrar().GetAppById(app_to_install->app_id()));
          // Add the app copy to the expected registry.
          registry.emplace(app_to_install->app_id(),
                           std::make_unique<WebApp>(*app_to_install));

          // Find the app in expected_apps_to_install set and remove the entry.
          bool found = false;
          for (const std::unique_ptr<WebApp>& expected_app_to_install :
               expected_apps_to_install) {
            if (expected_app_to_install->app_id() == app_to_install->app_id()) {
              EXPECT_EQ(*expected_app_to_install, *app_to_install);
              RemoveWebAppFromAppsList(&expected_apps_to_install,
                                       expected_app_to_install->app_id());
              found = true;
              break;
            }
          }
          EXPECT_TRUE(found);
        }

        EXPECT_TRUE(expected_apps_to_install.empty());

        // Run the callbacks to fulfill the contract.
        for (WebApp* app_to_install : apps_to_install) {
          callback.Run(app_to_install->app_id(),
                       InstallResultCode::kSuccessNewInstall);
        }

        run_loop.Quit();
      }));

  sync_bridge().MergeSyncData(sync_bridge().CreateMetadataChangeList(),
                              std::move(sync_data_list));
  run_loop.Run();

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

}  // namespace web_app
