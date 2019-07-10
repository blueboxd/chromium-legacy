// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_BRIDGE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_BRIDGE_H_

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace syncer {
class ModelTypeChangeProcessor;
}

namespace web_app {

class WebAppSyncBridge : public syncer::ModelTypeSyncBridge {
 public:
  explicit WebAppSyncBridge(
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor);
  ~WebAppSyncBridge() override;

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

 private:
  DISALLOW_COPY_AND_ASSIGN(WebAppSyncBridge);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_BRIDGE_H_
