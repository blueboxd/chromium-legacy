// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_sync_bridge.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_database.h"
#include "chrome/browser/web_applications/web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/common/channel_info.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "url/gurl.h"

namespace web_app {

namespace {

std::unique_ptr<syncer::EntityData> CreateSyncEntityData(const WebApp& app) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->name = app.name();

  sync_pb::WebAppSpecifics* specifics =
      entity_data->specifics.mutable_web_app();
  specifics->set_launch_url(app.launch_url().spec());
  specifics->set_name(app.name());
  specifics->set_display_mode(ToWebAppSpecificsDisplayMode(app.display_mode()));
  if (app.theme_color().has_value())
    specifics->set_theme_color(app.theme_color().value());

  return entity_data;
}

}  // namespace

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
  database_ = std::make_unique<WebAppDatabase>(
      database_factory,
      base::BindRepeating(&WebAppSyncBridge::ReportErrorToChangeProcessor,
                          base::Unretained(this)));
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

void WebAppSyncBridge::SetAppDisplayMode(
    const AppId& app_id,
    blink::mojom::DisplayMode display_mode) {
  ScopedRegistryUpdate update(this);
  WebApp* web_app = update->UpdateApp(app_id);
  if (web_app)
    web_app->SetDisplayMode(display_mode);
}

void WebAppSyncBridge::SetAppIsLocallyInstalledForTesting(
    const AppId& app_id,
    bool is_locally_installed) {
  ScopedRegistryUpdate update(this);
  WebApp* web_app = update->UpdateApp(app_id);
  if (web_app)
    web_app->SetIsLocallyInstalled(is_locally_installed);
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

void WebAppSyncBridge::OnDatabaseOpened(
    base::OnceClosure callback,
    Registry registry,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  // Provide sync metadata to the processor _before_ any local changes occur.
  change_processor()->ModelReadyToSync(std::move(metadata_batch));

  registrar_->InitRegistry(std::move(registry));
  std::move(callback).Run();
}

void WebAppSyncBridge::OnDataWritten(CommitCallback callback, bool success) {
  if (!success)
    DLOG(ERROR) << "WebAppSyncBridge commit failed";

  std::move(callback).Run(success);
}

void WebAppSyncBridge::ReportErrorToChangeProcessor(
    const syncer::ModelError& error) {
  change_processor()->ReportError(error);
}

std::unique_ptr<syncer::MetadataChangeList>
WebAppSyncBridge::CreateMetadataChangeList() {
  return syncer::ModelTypeStore::WriteBatch::CreateMetadataChangeList();
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
  auto data_batch = std::make_unique<syncer::MutableDataBatch>();

  for (const AppId& app_id : storage_keys) {
    const WebApp* app = registrar_->GetAppById(app_id);
    if (app && app->IsSynced())
      data_batch->Put(app->app_id(), CreateSyncEntityData(*app));
  }

  std::move(callback).Run(std::move(data_batch));
}

void WebAppSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  auto data_batch = std::make_unique<syncer::MutableDataBatch>();

  for (const WebApp& app : registrar_->AllApps()) {
    if (app.IsSynced())
      data_batch->Put(app.app_id(), CreateSyncEntityData(app));
  }

  std::move(callback).Run(std::move(data_batch));
}

std::string WebAppSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_web_app());

  const GURL launch_url(entity_data.specifics.web_app().launch_url());
  DCHECK(!launch_url.is_empty());
  DCHECK(launch_url.is_valid());

  return GenerateAppIdFromURL(launch_url);
}

std::string WebAppSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return GetClientTag(entity_data);
}

}  // namespace web_app
