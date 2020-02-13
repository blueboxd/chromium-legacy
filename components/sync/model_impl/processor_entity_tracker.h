// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_IMPL_PROCESSOR_ENTITY_TRACKER_H_
#define COMPONENTS_SYNC_MODEL_IMPL_PROCESSOR_ENTITY_TRACKER_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "components/sync/base/client_tag_hash.h"
#include "components/sync/engine/non_blocking_sync_common.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"

namespace syncer {

class ProcessorEntity;

// This component tracks entities for ClientTagBasedModelTypeProcessor.
class ProcessorEntityTracker {
 public:
  explicit ProcessorEntityTracker(ModelType type);
  ~ProcessorEntityTracker();

  // Returns true if all processor entities have non-empty storage keys.
  // This may happen during initial merge and for some data types during any
  // remote creation.
  bool AllStorageKeysPopulated() const;

  // Clears any in-memory sync state associated with outstanding commits
  // for each entity.
  void ClearTransientSyncState();

  // Returns number of entities with non-deleted metadata.
  size_t CountNonTombstoneEntries() const;

  // Creates new processor entity (must not be deleted outside current object).
  ProcessorEntity* Add(const std::string& storage_key, const EntityData& data);

  // Creates new processor entity loaded from storage (must not be deleted
  // outside current).
  // TODO(crbug.com/947044): use constructor to create current object from batch
  // data.
  ProcessorEntity* CreateEntityFromMetadata(const std::string& storage_key,
                                            sync_pb::EntityMetadata metadata);

  // Removes item from entities.
  void Remove(const ClientTagHash& client_tag_hash);

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;

  // Gets the entity for the given tag hash, or null if there isn't one.
  ProcessorEntity* GetEntityForTagHash(const ClientTagHash& tag_hash);
  const ProcessorEntity* GetEntityForTagHash(
      const ClientTagHash& tag_hash) const;

  // Returns all entities including tombstones.
  std::vector<const ProcessorEntity*> GetAllEntitiesIncludingTombstones() const;

  // Returns all entities with local changes.
  // TODO(rushans): make it const, at this moment returned entities must be
  // initialized to commit.
  std::vector<ProcessorEntity*> GetEntitiesWithLocalChanges(size_t max_entries);

  // Returns true if there are any local entities to be committed.
  bool HasLocalChanges() const;

  const sync_pb::ModelTypeState& model_type_state() const {
    return model_type_state_;
  }

  void set_model_type_state(const sync_pb::ModelTypeState& model_type_state) {
    model_type_state_ = model_type_state;
  }

  // Sets data type id to model type state. Used for first time syncing.
  void InitializeMetadata(ModelType type);

  // Returns number of entities, including tombstones.
  size_t size() const;

  // Increments sequence number for all entities except those in
  // |already_updated_storage_keys|. Returns affected list of entities.
  std::vector<const ProcessorEntity*> IncrementSequenceNumberForAllExcept(
      const std::unordered_set<std::string>& already_updated_storage_keys);

 private:
  // A map of client tag hash to sync entities known to this tracker. This
  // should contain entries and metadata, although the entities may not always
  // contain model type data/specifics.
  std::map<ClientTagHash, std::unique_ptr<ProcessorEntity>> entities_;

  // The model type metadata (progress marker, initial sync done, etc).
  sync_pb::ModelTypeState model_type_state_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_IMPL_PROCESSOR_ENTITY_TRACKER_H_
