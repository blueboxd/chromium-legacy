// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/processor_entity_tracker.h"

#include <utility>

#include "base/trace_event/memory_usage_estimator.h"
#include "components/sync/model_impl/processor_entity.h"
#include "components/sync/protocol/proto_memory_estimations.h"

namespace syncer {

ProcessorEntityTracker::ProcessorEntityTracker(ModelType type) {
  InitializeMetadata(type);
}

ProcessorEntityTracker::~ProcessorEntityTracker() = default;

bool ProcessorEntityTracker::AllStorageKeysPopulated() const {
  for (const auto& kv : entities_) {
    ProcessorEntity* entity = kv.second.get();
    if (entity->storage_key().empty())
      return false;
  }
  return true;
}

void ProcessorEntityTracker::ClearTransientSyncState() {
  for (const auto& kv : entities_) {
    kv.second->ClearTransientSyncState();
  }
}

size_t ProcessorEntityTracker::CountNonTombstoneEntries() const {
  size_t count = 0;
  for (const auto& kv : entities_) {
    if (!kv.second->metadata().is_deleted()) {
      ++count;
    }
  }
  return count;
}

ProcessorEntity* ProcessorEntityTracker::Add(const std::string& storage_key,
                                             const EntityData& data) {
  DCHECK(!data.client_tag_hash.value().empty());
  DCHECK(!GetEntityForTagHash(data.client_tag_hash));
  std::unique_ptr<ProcessorEntity> entity = ProcessorEntity::CreateNew(
      storage_key, data.client_tag_hash, data.id, data.creation_time);
  ProcessorEntity* entity_ptr = entity.get();
  entities_[data.client_tag_hash] = std::move(entity);
  return entity_ptr;
}

ProcessorEntity* ProcessorEntityTracker::CreateEntityFromMetadata(
    const std::string& storage_key,
    sync_pb::EntityMetadata metadata) {
  std::unique_ptr<ProcessorEntity> entity =
      ProcessorEntity::CreateFromMetadata(storage_key, std::move(metadata));
  ClientTagHash client_tag_hash =
      ClientTagHash::FromHashed(entity->metadata().client_tag_hash());
  ProcessorEntity* entity_ptr = entity.get();
  entities_[client_tag_hash] = std::move(entity);
  return entity_ptr;
}

void ProcessorEntityTracker::Remove(const ClientTagHash& client_tag_hash) {
  entities_.erase(client_tag_hash);
}

size_t ProcessorEntityTracker::EstimateMemoryUsage() const {
  size_t memory_usage = 0;
  memory_usage += sync_pb::EstimateMemoryUsage(model_type_state_);
  memory_usage += base::trace_event::EstimateMemoryUsage(entities_);
  return memory_usage;
}

ProcessorEntity* ProcessorEntityTracker::GetEntityForTagHash(
    const ClientTagHash& tag_hash) {
  return const_cast<ProcessorEntity*>(
      static_cast<const ProcessorEntityTracker*>(this)->GetEntityForTagHash(
          tag_hash));
}

const ProcessorEntity* ProcessorEntityTracker::GetEntityForTagHash(
    const ClientTagHash& tag_hash) const {
  auto it = entities_.find(tag_hash);
  return it != entities_.end() ? it->second.get() : nullptr;
}

std::vector<const ProcessorEntity*>
ProcessorEntityTracker::GetAllEntitiesIncludingTombstones() const {
  std::vector<const ProcessorEntity*> entities;
  entities.reserve(entities_.size());
  for (const auto& entity : entities_) {
    entities.push_back(entity.second.get());
  }
  return entities;
}

std::vector<ProcessorEntity*>
ProcessorEntityTracker::GetEntitiesWithLocalChanges(size_t max_entries) {
  std::vector<ProcessorEntity*> entities;
  for (const auto& kv : entities_) {
    ProcessorEntity* entity = kv.second.get();
    if (entity->RequiresCommitRequest() && !entity->RequiresCommitData()) {
      entities.push_back(entity);
      if (entities.size() >= max_entries)
        break;
    }
  }
  return entities;
}

bool ProcessorEntityTracker::HasLocalChanges() const {
  for (const auto& kv : entities_) {
    ProcessorEntity* entity = kv.second.get();
    if (entity->RequiresCommitRequest()) {
      return true;
    }
  }
  return false;
}

size_t ProcessorEntityTracker::size() const {
  return entities_.size();
}

void ProcessorEntityTracker::InitializeMetadata(ModelType type) {
  model_type_state_.mutable_progress_marker()->set_data_type_id(
      GetSpecificsFieldNumberFromModelType(type));
}

std::vector<const ProcessorEntity*>
ProcessorEntityTracker::IncrementSequenceNumberForAllExcept(
    const std::unordered_set<std::string>& already_updated_storage_keys) {
  std::vector<const ProcessorEntity*> affected_entities;
  for (const auto& kv : entities_) {
    ProcessorEntity* entity = kv.second.get();
    if (entity->storage_key().empty() ||
        (already_updated_storage_keys.find(entity->storage_key()) !=
         already_updated_storage_keys.end())) {
      // Entities with empty storage key were already processed. ProcessUpdate()
      // incremented their sequence numbers and cached commit data. Their
      // metadata will be persisted in UpdateStorageKey().
      continue;
    }
    entity->IncrementSequenceNumber(base::Time::Now());
    affected_entities.push_back(entity);
  }
  return affected_entities;
}

}  // namespace syncer
