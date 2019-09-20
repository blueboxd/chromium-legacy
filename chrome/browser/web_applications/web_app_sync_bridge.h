// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_BRIDGE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_BRIDGE_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_database.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/sync/model/model_type_sync_bridge.h"

class Profile;

namespace syncer {
class ModelTypeChangeProcessor;
}

namespace web_app {

class AbstractWebAppDatabaseFactory;
class WebAppRegistryUpdate;

// The sync bridge exclusively owns ModelTypeChangeProcessor and WebAppDatabase
// (the storage).
class WebAppSyncBridge : public AppRegistryController,
                         public syncer::ModelTypeSyncBridge {
 public:
  WebAppSyncBridge(Profile* profile,
                   AbstractWebAppDatabaseFactory* database_factory,
                   WebAppRegistrar* registrar);
  // Tests may inject mocks using this ctor.
  WebAppSyncBridge(
      Profile* profile,
      AbstractWebAppDatabaseFactory* database_factory,
      WebAppRegistrar* registrar,
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor);
  ~WebAppSyncBridge() override;

  // TODO(loyso): Erase UnregisterAll. Move Register/Unregister app methods to
  // BeginUpdate/CommitUpdate API.
  void RegisterApp(std::unique_ptr<WebApp> web_app);
  std::unique_ptr<WebApp> UnregisterApp(const AppId& app_id);
  void UnregisterAll();

  using CommitCallback = base::OnceCallback<void(bool success)>;
  // This is the writable API for the registry. Any updates will be written to
  // LevelDb and sync service. There can be only 1 update at a time.
  std::unique_ptr<WebAppRegistryUpdate> BeginUpdate();
  void CommitUpdate(std::unique_ptr<WebAppRegistryUpdate> update,
                    CommitCallback callback);

  // AppRegistryController:
  void Init(base::OnceClosure callback) override;
  void SetAppLaunchContainer(const AppId& app_id,
                             LaunchContainer launch_container) override;
  WebAppSyncBridge* AsWebAppSyncBridge() override;

  WebAppDatabase& database_for_testing() { return *database_; }

 private:
  friend class WebAppRegistryUpdate;

  WebApp* GetAppByIdMutable(const AppId& app_id);

  void OnDatabaseOpened(base::OnceClosure callback, Registry registry);
  void OnDataWritten(CommitCallback callback, bool success);

  // syncer::ModelTypeSyncBridge:
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  base::Optional<syncer::ModelError> MergeSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  base::Optional<syncer::ModelError> ApplySyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;

  std::unique_ptr<WebAppDatabase> database_;
  WebAppRegistrar* registrar_;

  bool is_in_update_ = false;

  base::WeakPtrFactory<WebAppSyncBridge> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppSyncBridge);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_BRIDGE_H_
