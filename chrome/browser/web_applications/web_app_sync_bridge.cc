// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_sync_bridge.h"

#include "base/logging.h"
#include "base/optional.h"
#include "components/sync/model/metadata_change_list.h"

namespace web_app {

WebAppSyncBridge::WebAppSyncBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : syncer::ModelTypeSyncBridge(std::move(change_processor)) {}

WebAppSyncBridge::~WebAppSyncBridge() = default;

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
