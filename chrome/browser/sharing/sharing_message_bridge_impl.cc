// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_message_bridge_impl.h"

#include "base/guid.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/sharing/features.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model_impl/dummy_metadata_change_list.h"
#include "net/base/network_change_notifier.h"

namespace {

void ReplyToCallback(SharingMessageBridge::CommitFinishedCallback callback,
                     const sync_pb::SharingMessageCommitError& commit_error) {
  DCHECK(commit_error.has_error_code());
  base::UmaHistogramExactLinear(
      "Sync.SharingMessage.CommitResult", commit_error.error_code(),
      sync_pb::SharingMessageCommitError::ErrorCode_ARRAYSIZE);
  std::move(callback).Run(commit_error);
}

void ReplyToCallback(
    SharingMessageBridge::CommitFinishedCallback callback,
    sync_pb::SharingMessageCommitError::ErrorCode commit_error_code) {
  sync_pb::SharingMessageCommitError error_message;
  error_message.set_error_code(commit_error_code);
  ReplyToCallback(std::move(callback), error_message);
}

syncer::ClientTagHash GetClientTagHashFromStorageKey(
    const std::string& storage_key) {
  return syncer::ClientTagHash::FromUnhashed(syncer::SHARING_MESSAGE,
                                             storage_key);
}

std::unique_ptr<syncer::EntityData> MoveToEntityData(
    std::unique_ptr<sync_pb::SharingMessageSpecifics> specifics) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->name = specifics->message_id();
  entity_data->client_tag_hash =
      GetClientTagHashFromStorageKey(specifics->message_id());
  entity_data->specifics.set_allocated_sharing_message(specifics.release());
  return entity_data;
}

}  // namespace

SharingMessageBridgeImpl::SharingMessageBridgeImpl(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : ModelTypeSyncBridge(std::move(change_processor)) {
  // Current data type doesn't have persistent storage so it's ready to sync
  // immediately.
  this->change_processor()->ModelReadyToSync(
      std::make_unique<syncer::MetadataBatch>());
}

SharingMessageBridgeImpl::~SharingMessageBridgeImpl() = default;

void SharingMessageBridgeImpl::SendSharingMessage(
    std::unique_ptr<sync_pb::SharingMessageSpecifics> specifics,
    CommitFinishedCallback on_commit_callback) {
  if (!change_processor()->IsTrackingMetadata()) {
    ReplyToCallback(std::move(on_commit_callback),
                    sync_pb::SharingMessageCommitError::SYNC_TURNED_OFF);
    return;
  }

  if (net::NetworkChangeNotifier::GetConnectionType() ==
      net::NetworkChangeNotifier::CONNECTION_NONE) {
    ReplyToCallback(std::move(on_commit_callback),
                    sync_pb::SharingMessageCommitError::SYNC_NETWORK_ERROR);
    return;
  }

  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();
  // Fill in the internal message id with unique generated identifier.
  const std::string message_id = base::GenerateGUID();
  specifics->set_message_id(message_id);
  std::unique_ptr<syncer::EntityData> entity_data =
      MoveToEntityData(std::move(specifics));
  const syncer::ClientTagHash client_tag_hash =
      GetClientTagHashFromStorageKey(message_id);
  const auto result = commit_callbacks_.emplace(
      client_tag_hash,
      std::make_unique<TimedCallback>(
          std::move(on_commit_callback),
          base::BindOnce(&SharingMessageBridgeImpl::ProcessCommitTimeout,
                         base::Unretained(this), client_tag_hash)));
  DCHECK(result.second);
  change_processor()->Put(message_id, std::move(entity_data),
                          metadata_change_list.get());
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
SharingMessageBridgeImpl::GetControllerDelegate() {
  return change_processor()->GetControllerDelegate();
}

std::unique_ptr<syncer::MetadataChangeList>
SharingMessageBridgeImpl::CreateMetadataChangeList() {
  // The data type intentionally doesn't persist the data on disk, so metadata
  // is just ignored.
  return std::make_unique<syncer::DummyMetadataChangeList>();
}

base::Optional<syncer::ModelError> SharingMessageBridgeImpl::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK(entity_data.empty());
  DCHECK(change_processor()->IsTrackingMetadata());
  return {};
}

base::Optional<syncer::ModelError> SharingMessageBridgeImpl::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  sync_pb::SharingMessageCommitError no_error_message;
  no_error_message.set_error_code(sync_pb::SharingMessageCommitError::NONE);
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    // For commit-only data type we expect only |ACTION_DELETE| changes.
    DCHECK_EQ(syncer::EntityChange::ACTION_DELETE, change->type());

    const syncer::ClientTagHash client_tag_hash =
        GetClientTagHashFromStorageKey(change->storage_key());
    ProcessCommitResponse(client_tag_hash, no_error_message);
  }
  return {};
}

void SharingMessageBridgeImpl::GetData(StorageKeyList storage_keys,
                                       DataCallback callback) {
  return GetAllDataForDebugging(std::move(callback));
}

void SharingMessageBridgeImpl::GetAllDataForDebugging(DataCallback callback) {
  // This data type does not store any data, we can always run the callback
  // with empty data.
  std::move(callback).Run(std::make_unique<syncer::MutableDataBatch>());
}

std::string SharingMessageBridgeImpl::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string SharingMessageBridgeImpl::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_sharing_message());
  return entity_data.specifics.sharing_message().message_id();
}

void SharingMessageBridgeImpl::OnCommitAttemptErrors(
    const syncer::FailedCommitResponseDataList& error_response_list) {
  for (const syncer::FailedCommitResponseData& response : error_response_list) {
    // We do not want to retry committing again, thus, the bridge has to untrack
    // the failed item.
    change_processor()->UntrackEntityForClientTagHash(response.client_tag_hash);
    ProcessCommitResponse(
        response.client_tag_hash,
        response.datatype_specific_error.sharing_message_error());
  }
}

void SharingMessageBridgeImpl::OnCommitAttemptFailed(
    syncer::SyncCommitError commit_error) {
  // Full commit failed means we need to drop all entities and report an error
  // using callback.
  sync_pb::SharingMessageCommitError::ErrorCode sharing_message_error_code;
  switch (commit_error) {
    case syncer::SyncCommitError::kNetworkError:
      sharing_message_error_code =
          sync_pb::SharingMessageCommitError::SYNC_NETWORK_ERROR;
      break;
    case syncer::SyncCommitError::kServerError:
    case syncer::SyncCommitError::kBadServerResponse:
      sharing_message_error_code =
          sync_pb::SharingMessageCommitError::SYNC_SERVER_ERROR;
      break;
  }

  sync_pb::SharingMessageCommitError sync_error_message;
  sync_error_message.set_error_code(sharing_message_error_code);
  for (auto& cth_and_callback : commit_callbacks_) {
    change_processor()->UntrackEntityForClientTagHash(cth_and_callback.first);
    cth_and_callback.second->Run(sync_error_message);
  }
  commit_callbacks_.clear();
}

void SharingMessageBridgeImpl::ApplyStopSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list) {
  sync_pb::SharingMessageCommitError sync_disabled_error_message;
  sync_disabled_error_message.set_error_code(
      sync_pb::SharingMessageCommitError::SYNC_TURNED_OFF);
  for (auto& cth_and_callback : commit_callbacks_) {
    change_processor()->UntrackEntityForClientTagHash(cth_and_callback.first);
    cth_and_callback.second->Run(sync_disabled_error_message);
  }
  commit_callbacks_.clear();
}

void SharingMessageBridgeImpl::ProcessCommitTimeout(
    syncer::ClientTagHash client_tag_hash) {
  change_processor()->UntrackEntityForClientTagHash(client_tag_hash);
  sync_pb::SharingMessageCommitError error_message;
  error_message.set_error_code(
      sync_pb::SharingMessageCommitError::SYNC_TIMEOUT);
  ProcessCommitResponse(client_tag_hash, error_message);
}

void SharingMessageBridgeImpl::ProcessCommitResponse(
    const syncer::ClientTagHash& client_tag_hash,
    const sync_pb::SharingMessageCommitError& commit_error_message) {
  const auto iter = commit_callbacks_.find(client_tag_hash);
  if (iter == commit_callbacks_.end()) {
    // This may happen if tasks from OnUpdateReceived and OneShotTimer were
    // added at one time.
    return;
  }
  iter->second->Run(commit_error_message);
  commit_callbacks_.erase(iter);
}

SharingMessageBridgeImpl::TimedCallback::TimedCallback(
    CommitFinishedCallback commit_callback,
    base::OnceClosure timeout_callback)
    : commit_callback_(std::move(commit_callback)) {
  const base::TimeDelta time_delta =
      base::TimeDelta::FromSeconds(kSharingMessageBridgeTimeoutSeconds.Get());
  timer_.Start(FROM_HERE, time_delta, std::move(timeout_callback));
}

SharingMessageBridgeImpl::TimedCallback::~TimedCallback() = default;

void SharingMessageBridgeImpl::TimedCallback::Run(
    const sync_pb::SharingMessageCommitError& commit_error) {
  DCHECK(commit_callback_);

  ReplyToCallback(std::move(commit_callback_), commit_error);
  // |timer_| may be already stopped if Run is called from
  // ProcessCommitTimeout.
  if (timer_.IsRunning()) {
    timer_.Stop();
  }
}
