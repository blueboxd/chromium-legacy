// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_sync_bridge.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/web_app_database.h"
#include "chrome/browser/web_applications/web_app_database_factory.h"
#include "chrome/common/channel_info.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"

namespace web_app {

WebAppSyncBridge::WebAppSyncBridge(
    AbstractWebAppDatabaseFactory* database_factory)
    : WebAppSyncBridge(
          database_factory,
          std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
              syncer::WEB_APPS,
              base::BindRepeating(&syncer::ReportUnrecoverableError,
                                  chrome::GetChannel()))) {}

WebAppSyncBridge::WebAppSyncBridge(
    AbstractWebAppDatabaseFactory* database_factory,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : syncer::ModelTypeSyncBridge(std::move(change_processor)) {
  DCHECK(database_factory);
  database_ = std::make_unique<WebAppDatabase>(database_factory);
}

WebAppSyncBridge::~WebAppSyncBridge() = default;

void WebAppSyncBridge::OpenDatabase(RegistryOpenedCallback callback) {
  database_->OpenDatabase(std::move(callback));
}

void WebAppSyncBridge::WriteWebApps(AppsToWrite apps,
                                    CompletionCallback callback) {
  database_->WriteWebApps(std::move(apps), std::move(callback));
}

void WebAppSyncBridge::DeleteWebApps(std::vector<AppId> app_ids,
                                     CompletionCallback callback) {
  database_->DeleteWebApps(std::move(app_ids), std::move(callback));
}

void WebAppSyncBridge::ReadRegistry(RegistryOpenedCallback callback) {
  database_->ReadRegistry(std::move(callback));
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
