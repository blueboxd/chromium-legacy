// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_IMPL_CLIENT_TAG_BASED_REMOTE_UPDATE_HANDLER_H_
#define COMPONENTS_SYNC_MODEL_IMPL_CLIENT_TAG_BASED_REMOTE_UPDATE_HANDLER_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_set>

#include "base/macros.h"
#include "base/optional.h"

#include "components/sync/engine/non_blocking_sync_common.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/model_error.h"

namespace sync_pb {
class ModelTypeState;
}  // namespace sync_pb

namespace syncer {

class MetadataChangeList;
class ModelTypeSyncBridge;
class ProcessorEntity;

// A sync component that performs updates from sync server.
class ClientTagBasedRemoteUpdateHandler {
 public:
  // All parameters must not be nullptr and they must outlive this object.
  // |model_type_state|, |storage_key_to_tag_hash| and |entities| are
  // ClientTagBasedModelTypeProcessor internal fields. This will be changed in
  // future.
  ClientTagBasedRemoteUpdateHandler(
      ModelType type,
      ModelTypeSyncBridge* bridge,
      sync_pb::ModelTypeState* model_type_state,
      std::map<std::string, ClientTagHash>* storage_key_to_tag_hash,
      std::map<ClientTagHash, std::unique_ptr<ProcessorEntity>>* entities);

  // Processes incremental updates from the sync server.
  base::Optional<ModelError> ProcessIncrementalUpdate(
      const sync_pb::ModelTypeState& model_type_state,
      UpdateResponseDataList updates);

  ClientTagBasedRemoteUpdateHandler(const ClientTagBasedRemoteUpdateHandler&) =
      delete;
  ClientTagBasedRemoteUpdateHandler& operator=(
      const ClientTagBasedRemoteUpdateHandler&) = delete;

 private:
  // Helper function to process the update for a single entity. If a local data
  // change is required, it will be added to |entity_changes|. The return value
  // is the tracked entity, or nullptr if the update should be ignored.
  // |storage_key_to_clear| must not be null and allows the implementation to
  // indicate that a certain storage key is now obsolete and should be cleared,
  // which is leveraged in certain conflict resolution scenarios.
  ProcessorEntity* ProcessUpdate(UpdateResponseData update,
                                 EntityChangeList* entity_changes,
                                 std::string* storage_key_to_clear);

  // Recommit all entities for encryption except those in |already_updated|.
  void RecommitAllForEncryption(
      const std::unordered_set<std::string>& already_updated,
      MetadataChangeList* metadata_changes);

  // Resolve a conflict between |update| and the pending commit in |entity|.
  ConflictResolution ResolveConflict(UpdateResponseData update,
                                     ProcessorEntity* entity,
                                     EntityChangeList* changes,
                                     std::string* storage_key_to_clear);

  // Gets the entity for the given tag hash, or null if there isn't one.
  ProcessorEntity* GetEntityForTagHash(const ClientTagHash& tag_hash);

  // Create an entity in the entity map for |storage_key|.
  // |storage_key| must not exist in |storage_key_to_tag_hash_|.
  ProcessorEntity* CreateEntity(const std::string& storage_key,
                                const EntityData& data);

  // Version of the above that generates a tag for |data|.
  ProcessorEntity* CreateEntity(const EntityData& data);

  // The model type this object syncs.
  const ModelType type_;

  // ModelTypeSyncBridge linked to associated processor.
  ModelTypeSyncBridge* const bridge_;

  // The model type metadata (progress marker, initial sync done, etc).
  sync_pb::ModelTypeState* const model_type_state_;

  // This mapping allows us to convert from storage key to client tag hash.
  // Should be replaced with new interface.
  std::map<std::string, ClientTagHash>* const storage_key_to_tag_hash_;

  // A map of client tag hash to sync entities known to the processor.
  // Should be replaced with new interface.
  std::map<ClientTagHash, std::unique_ptr<ProcessorEntity>>* const entities_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_IMPL_CLIENT_TAG_BASED_REMOTE_UPDATE_HANDLER_H_
