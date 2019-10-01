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
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/sync/model/model_type_sync_bridge.h"

class Profile;

namespace syncer {
class MetadataBatch;
class ModelError;
class ModelTypeChangeProcessor;
}  // namespace syncer

namespace web_app {

class AbstractWebAppDatabaseFactory;
class WebAppDatabase;
class WebAppRegistryUpdate;
struct RegistryUpdateData;

// The sync bridge exclusively owns ModelTypeChangeProcessor and WebAppDatabase
// (the storage).
class WebAppSyncBridge : public AppRegistryController,
                         public syncer::ModelTypeSyncBridge {
 public:
  WebAppSyncBridge(Profile* profile,
                   AbstractWebAppDatabaseFactory* database_factory,
                   WebAppRegistrarMutable* registrar);
  // Tests may inject mocks using this ctor.
  WebAppSyncBridge(
      Profile* profile,
      AbstractWebAppDatabaseFactory* database_factory,
      WebAppRegistrarMutable* registrar,
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor);
  ~WebAppSyncBridge() override;

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
  void SetAppIsLocallyInstalledForTesting(const AppId& app_id,
                                          bool is_locally_installed) override;
  WebAppSyncBridge* AsWebAppSyncBridge() override;

 private:
  void CheckRegistryUpdateData(const RegistryUpdateData& update_data) const;
  // Update the in-memory model.
  void UpdateRegistrar(std::unique_ptr<RegistryUpdateData> update_data);

  void OnDatabaseOpened(base::OnceClosure callback,
                        Registry registry,
                        std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void OnDataWritten(CommitCallback callback, bool success);

  void ReportErrorToChangeProcessor(const syncer::ModelError& error);

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
  WebAppRegistrarMutable* registrar_;

  bool is_in_update_ = false;

  base::WeakPtrFactory<WebAppSyncBridge> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppSyncBridge);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_BRIDGE_H_
