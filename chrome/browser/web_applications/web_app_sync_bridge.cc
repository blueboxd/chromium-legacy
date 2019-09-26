// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_sync_bridge.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_database.h"
#include "chrome/browser/web_applications/web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/common/channel_info.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"

namespace web_app {

WebAppSyncBridge::WebAppSyncBridge(
    Profile* profile,
    AbstractWebAppDatabaseFactory* database_factory,
    WebAppRegistrarMutable* registrar)
    : WebAppSyncBridge(
          profile,
          database_factory,
          registrar,
          std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
              syncer::WEB_APPS,
              base::BindRepeating(&syncer::ReportUnrecoverableError,
                                  chrome::GetChannel()))) {}

WebAppSyncBridge::WebAppSyncBridge(
    Profile* profile,
    AbstractWebAppDatabaseFactory* database_factory,
    WebAppRegistrarMutable* registrar,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : AppRegistryController(profile),
      syncer::ModelTypeSyncBridge(std::move(change_processor)),
      registrar_(registrar) {
  DCHECK(database_factory);
  DCHECK(registrar_);
  database_ = std::make_unique<WebAppDatabase>(database_factory);
}

WebAppSyncBridge::~WebAppSyncBridge() = default;

std::unique_ptr<WebAppRegistryUpdate> WebAppSyncBridge::BeginUpdate() {
  DCHECK(!is_in_update_);
  is_in_update_ = true;

  database_->BeginTransaction();

  return std::make_unique<WebAppRegistryUpdate>(registrar_);
}

void WebAppSyncBridge::CommitUpdate(
    std::unique_ptr<WebAppRegistryUpdate> update,
    CommitCallback callback) {
  DCHECK(is_in_update_);
  is_in_update_ = false;

  if (update == nullptr || update->update_data().IsEmpty()) {
    database_->CancelTransaction();
    std::move(callback).Run(/*success*/ true);
    return;
  }

  CheckRegistryUpdateData(update->update_data());

  registrar_->CountMutation();

  std::unique_ptr<RegistryUpdateData> update_data = update->TakeUpdateData();

  database_->CommitTransaction(
      *update_data,
      base::BindOnce(&WebAppSyncBridge::OnDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  UpdateRegistrar(std::move(update_data));
}

void WebAppSyncBridge::Init(base::OnceClosure callback) {
  database_->OpenDatabase(base::BindOnce(&WebAppSyncBridge::OnDatabaseOpened,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         std::move(callback)));
}

void WebAppSyncBridge::SetAppLaunchContainer(const AppId& app_id,
                                             LaunchContainer launch_container) {
  ScopedRegistryUpdate update(this);
  WebApp* web_app = update->UpdateApp(app_id);
  if (web_app)
    web_app->SetLaunchContainer(launch_container);
}

WebAppSyncBridge* WebAppSyncBridge::AsWebAppSyncBridge() {
  return this;
}

void WebAppSyncBridge::CheckRegistryUpdateData(
    const RegistryUpdateData& update_data) const {
#if DCHECK_IS_ON()
  for (const std::unique_ptr<WebApp>& web_app : update_data.apps_to_create)
    DCHECK(!registrar_->GetAppById(web_app->app_id()));

  for (const AppId& app_id : update_data.apps_to_delete)
    DCHECK(registrar_->GetAppById(app_id));

  for (const WebApp* web_app : update_data.apps_to_update)
    DCHECK(registrar_->GetAppById(web_app->app_id()));
#endif
}

void WebAppSyncBridge::UpdateRegistrar(
    std::unique_ptr<RegistryUpdateData> update_data) {
  for (std::unique_ptr<WebApp>& web_app : update_data->apps_to_create) {
    AppId app_id = web_app->app_id();
    DCHECK(!registrar_->GetAppById(app_id));
    registrar_->registry().emplace(std::move(app_id), std::move(web_app));
  }

  for (const AppId& app_id : update_data->apps_to_delete) {
    auto it = registrar_->registry().find(app_id);
    DCHECK(it != registrar_->registry().end());
    registrar_->registry().erase(it);
  }
}

void WebAppSyncBridge::OnDatabaseOpened(base::OnceClosure callback,
                                        Registry registry) {
  registrar_->InitRegistry(std::move(registry));
  std::move(callback).Run();
}

void WebAppSyncBridge::OnDataWritten(CommitCallback callback, bool success) {
  if (!success)
    DLOG(ERROR) << "WebAppSyncBridge commit failed";

  std::move(callback).Run(success);
}

std::unique_ptr<syncer::MetadataChangeList>
WebAppSyncBridge::CreateMetadataChangeList() {
  NOTIMPLEMENTED();
  return nullptr;
}

base::Optional<syncer::ModelError> WebAppSyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  NOTIMPLEMENTED();
  return base::nullopt;
}

base::Optional<syncer::ModelError> WebAppSyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  NOTIMPLEMENTED();
  return base::nullopt;
}

void WebAppSyncBridge::GetData(StorageKeyList storage_keys,
                               DataCallback callback) {
  NOTIMPLEMENTED();
}

void WebAppSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  NOTIMPLEMENTED();
}

std::string WebAppSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  NOTIMPLEMENTED();
  return std::string();
}

std::string WebAppSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  NOTIMPLEMENTED();
  return std::string();
}

}  // namespace web_app
