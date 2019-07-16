// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_server/fake_server.h"

#include <algorithm>
#include <limits>
#include <set>
#include <utility>

#include "base/guid.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "components/sync/engine_impl/net/server_connection_manager.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"

using syncer::GetModelType;
using syncer::GetModelTypeFromSpecifics;
using syncer::LoopbackServer;
using syncer::LoopbackServerEntity;
using syncer::ModelType;
using syncer::ModelTypeSet;

namespace fake_server {

FakeServer::FakeServer()
    : error_type_(sync_pb::SyncEnums::SUCCESS),
      alternate_triggered_errors_(false),
      request_counter_(0) {
  base::ThreadRestrictions::SetIOAllowed(true);
  loopback_server_storage_ = std::make_unique<base::ScopedTempDir>();
  if (!loopback_server_storage_->CreateUniqueTempDir()) {
    NOTREACHED() << "Creating temp dir failed.";
  }
  loopback_server_ = std::make_unique<syncer::LoopbackServer>(
      loopback_server_storage_->GetPath().AppendASCII("profile.pb"));
  loopback_server_->set_observer_for_tests(this);
}

FakeServer::FakeServer(const base::FilePath& user_data_dir)
    : error_type_(sync_pb::SyncEnums::SUCCESS),
      alternate_triggered_errors_(false),
      request_counter_(0) {
  base::ThreadRestrictions::SetIOAllowed(true);
  base::FilePath loopback_server_path =
      user_data_dir.AppendASCII("FakeSyncServer");
  loopback_server_ = std::make_unique<syncer::LoopbackServer>(
      loopback_server_path.AppendASCII("profile.pb"));
  loopback_server_->set_observer_for_tests(this);
}

FakeServer::~FakeServer() {}

namespace {

std::unique_ptr<sync_pb::DataTypeProgressMarker>
RemoveWalletProgressMarkerIfExists(sync_pb::ClientToServerMessage* message) {
  google::protobuf::RepeatedPtrField<sync_pb::DataTypeProgressMarker>*
      progress_markers =
          message->mutable_get_updates()->mutable_from_progress_marker();
  for (int index = 0; index < progress_markers->size(); ++index) {
    if (syncer::GetModelTypeFromSpecificsFieldNumber(
            progress_markers->Get(index).data_type_id()) ==
        syncer::AUTOFILL_WALLET_DATA) {
      auto result = std::make_unique<sync_pb::DataTypeProgressMarker>(
          progress_markers->Get(index));
      progress_markers->erase(progress_markers->begin() + index);
      return result;
    }
  }
  return nullptr;
}

void VerifyNoWalletDataProgressMarkerExists(
    sync_pb::GetUpdatesResponse* gu_response) {
  for (const sync_pb::DataTypeProgressMarker& marker :
       gu_response->new_progress_marker()) {
    DCHECK_NE(
        syncer::GetModelTypeFromSpecificsFieldNumber(marker.data_type_id()),
        syncer::AUTOFILL_WALLET_DATA);
  }
}

std::string GetTokenFromHashAndTime(int64_t hash, const base::Time& time) {
  return base::NumberToString(hash) + " " +
         base::NumberToString(time.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

int64_t GetHashFromToken(const std::string& token, int64_t default_value) {
  // The hash is stored as a first piece of the string (space delimited), the
  // second piece is the timestamp.
  std::vector<base::StringPiece> pieces =
      base::SplitStringPiece(token, base::kWhitespaceASCII,
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  int64_t hash = 0;
  if (pieces.size() != 2 || !base::StringToInt64(pieces[0], &hash)) {
    return default_value;
  }
  return hash;
}

void PopulateWalletResults(
    const std::vector<sync_pb::SyncEntity>& entities,
    const sync_pb::DataTypeProgressMarker& old_wallet_marker,
    sync_pb::GetUpdatesResponse* gu_response) {
  // The response from the loopback server should never have an existing
  // progress marker for wallet data (because FakeServer removes it from the
  // request).
  VerifyNoWalletDataProgressMarkerExists(gu_response);
  sync_pb::DataTypeProgressMarker* new_wallet_marker =
      gu_response->add_new_progress_marker();
  new_wallet_marker->set_data_type_id(
      GetSpecificsFieldNumberFromModelType(syncer::AUTOFILL_WALLET_DATA));

  // Make sure to pick a token that will be consistent across clients when
  // receiving the same data. We sum up the hashes which has the nice side
  // effect of being independent of the order.
  // We also include information about the fetch time in the token. This is
  // in-line with the server behavior and -- as it keeps changing -- allows
  // integration tests to wait for a GetUpdates call to finish, even if they
  // don't contain data updates.
  int64_t hash = 0;
  for (const auto& entity : entities) {
    // PersistentHash returns 32-bit integers, so summing them up is defined
    // behavior.
    hash += base::PersistentHash(entity.id_string());
  }
  std::string token = GetTokenFromHashAndTime(hash, base::Time::Now());
  new_wallet_marker->set_token(token);

  if (!old_wallet_marker.has_token() ||
      !AreWalletDataProgressMarkersEquivalent(old_wallet_marker,
                                              *new_wallet_marker)) {
    // New data available; include new elements and tell the client to drop all
    // previous data.
    int64_t version =
        (base::Time::Now() - base::Time::UnixEpoch()).InMilliseconds();
    for (const auto& entity : entities) {
      sync_pb::SyncEntity* response_entity = gu_response->add_entries();
      *response_entity = entity;
      response_entity->set_version(version);
    }

    // Set the GC directive to implement non-incremental reads.
    new_wallet_marker->mutable_gc_directive()->set_type(
        sync_pb::GarbageCollectionDirective::VERSION_WATERMARK);
    new_wallet_marker->mutable_gc_directive()->set_version_watermark(version -
                                                                     1);
  }
}

}  // namespace

bool AreWalletDataProgressMarkersEquivalent(
    const sync_pb::DataTypeProgressMarker& marker1,
    const sync_pb::DataTypeProgressMarker& marker2) {
  return GetHashFromToken(marker1.token(), /*default_value=*/-1) ==
         GetHashFromToken(marker2.token(), /*default_value=*/-1);
}

net::HttpStatusCode FakeServer::HandleCommand(const std::string& request,
                                              std::string* response) {
  DCHECK(thread_checker_.CalledOnValidThread());
  response->clear();

  request_counter_++;

  if (http_error_status_code_) {
    return *http_error_status_code_;
  }

  sync_pb::ClientToServerResponse response_proto;
  if (error_type_ != sync_pb::SyncEnums::SUCCESS &&
      ShouldSendTriggeredError()) {
    response_proto.set_error_code(error_type_);
  } else if (triggered_actionable_error_.get() && ShouldSendTriggeredError()) {
    sync_pb::ClientToServerResponse_Error* error =
        response_proto.mutable_error();
    error->CopyFrom(*(triggered_actionable_error_.get()));
  } else {
    sync_pb::ClientToServerMessage message;
    bool parsed = message.ParseFromString(request);
    DCHECK(parsed) << "Unable to parse the ClientToServerMessage.";
    switch (message.message_contents()) {
      case sync_pb::ClientToServerMessage::GET_UPDATES:
        last_getupdates_message_ = message;
        break;
      case sync_pb::ClientToServerMessage::COMMIT:
        last_commit_message_ = message;
        break;
      default:
        break;
        // Don't care.
    }
    // The loopback server does not know how to handle Wallet requests -- and
    // should not. The FakeServer is handling those instead. The loopback server
    // has a strong expectations about how progress tokens are structured. To
    // not interfere with this, we remove wallet progress markers before passing
    // the request to the loopback server and add them back again afterwards
    // before handling those requests.
    std::unique_ptr<sync_pb::DataTypeProgressMarker> wallet_marker =
        RemoveWalletProgressMarkerIfExists(&message);
    net::HttpStatusCode http_status_code =
        SendToLoopbackServer(message.SerializeAsString(), response);
    if (wallet_marker != nullptr) {
      *message.mutable_get_updates()->add_from_progress_marker() =
          *wallet_marker;
      if (http_status_code == net::HTTP_OK) {
        HandleWalletRequest(message, *wallet_marker, response);
      }
    }
    if (http_status_code == net::HTTP_OK) {
      InjectClientCommand(response);
    }
    return http_status_code;
  }

  response_proto.set_store_birthday(loopback_server_->GetStoreBirthday());
  *response = response_proto.SerializeAsString();
  return net::HTTP_OK;
}

void FakeServer::HandleWalletRequest(
    const sync_pb::ClientToServerMessage& request,
    const sync_pb::DataTypeProgressMarker& old_wallet_marker,
    std::string* response_string) {
  if (request.message_contents() !=
      sync_pb::ClientToServerMessage::GET_UPDATES) {
    return;
  }
  sync_pb::ClientToServerResponse response_proto;
  CHECK(response_proto.ParseFromString(*response_string));
  PopulateWalletResults(wallet_entities_, old_wallet_marker,
                        response_proto.mutable_get_updates());
  *response_string = response_proto.SerializeAsString();
}

net::HttpStatusCode FakeServer::SendToLoopbackServer(const std::string& request,
                                                     std::string* response) {
  base::ThreadRestrictions::SetIOAllowed(true);
  return loopback_server_->HandleCommand(request, response);
}

void FakeServer::InjectClientCommand(std::string* response) {
  sync_pb::ClientToServerResponse response_proto;
  bool parse_ok = response_proto.ParseFromString(*response);
  DCHECK(parse_ok) << "Unable to parse-back the server response";
  if (response_proto.error_code() == sync_pb::SyncEnums::SUCCESS) {
    *response_proto.mutable_client_command() = client_command_;
    *response = response_proto.SerializeAsString();
  }
}

bool FakeServer::GetLastCommitMessage(sync_pb::ClientToServerMessage* message) {
  if (!last_commit_message_.has_commit())
    return false;

  message->CopyFrom(last_commit_message_);
  return true;
}

bool FakeServer::GetLastGetUpdatesMessage(
    sync_pb::ClientToServerMessage* message) {
  if (!last_getupdates_message_.has_get_updates())
    return false;

  message->CopyFrom(last_getupdates_message_);
  return true;
}

void FakeServer::OverrideResponseType(
    LoopbackServer::ResponseTypeProvider response_type_override) {
  loopback_server_->OverrideResponseType(std::move(response_type_override));
}

std::unique_ptr<base::DictionaryValue>
FakeServer::GetEntitiesAsDictionaryValue() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return loopback_server_->GetEntitiesAsDictionaryValue();
}

std::vector<sync_pb::SyncEntity> FakeServer::GetSyncEntitiesByModelType(
    ModelType model_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return loopback_server_->GetSyncEntitiesByModelType(model_type);
}

std::vector<sync_pb::SyncEntity>
FakeServer::GetPermanentSyncEntitiesByModelType(ModelType model_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return loopback_server_->GetPermanentSyncEntitiesByModelType(model_type);
}

std::string FakeServer::GetTopLevelPermanentItemId(
    syncer::ModelType model_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return loopback_server_->GetTopLevelPermanentItemId(model_type);
}

const std::vector<std::string>& FakeServer::GetKeystoreKeys() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return loopback_server_->GetKeystoreKeysForTesting();
}

void FakeServer::InjectEntity(std::unique_ptr<LoopbackServerEntity> entity) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(entity->GetModelType() != syncer::AUTOFILL_WALLET_DATA)
      << "Wallet data must be injected via SetWalletData()";
  loopback_server_->SaveEntity(std::move(entity));
}

void FakeServer::SetWalletData(
    const std::vector<sync_pb::SyncEntity>& wallet_entities) {
  for (const auto& entity : wallet_entities) {
    DCHECK_EQ(GetModelTypeFromSpecifics(entity.specifics()),
              syncer::AUTOFILL_WALLET_DATA);
    DCHECK(!entity.has_client_defined_unique_tag())
        << "The sync server doesn not provide a client tag for wallet entries";
    DCHECK(!entity.id_string().empty()) << "server id required!";
  }
  wallet_entities_ = wallet_entities;
}

bool FakeServer::ModifyEntitySpecifics(
    const std::string& id,
    const sync_pb::EntitySpecifics& updated_specifics) {
  return loopback_server_->ModifyEntitySpecifics(id, updated_specifics);
}

bool FakeServer::ModifyBookmarkEntity(
    const std::string& id,
    const std::string& parent_id,
    const sync_pb::EntitySpecifics& updated_specifics) {
  return loopback_server_->ModifyBookmarkEntity(id, parent_id,
                                                updated_specifics);
}

void FakeServer::ClearServerData() {
  DCHECK(thread_checker_.CalledOnValidThread());
  loopback_server_->ClearServerData();
}

void FakeServer::SetHttpError(net::HttpStatusCode http_status_code) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GT(http_status_code, 0);
  http_error_status_code_ = http_status_code;
}

void FakeServer::ClearHttpError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  http_error_status_code_ = base::nullopt;
}

void FakeServer::SetClientCommand(
    const sync_pb::ClientCommand& client_command) {
  DCHECK(thread_checker_.CalledOnValidThread());
  client_command_ = client_command;
}

bool FakeServer::TriggerError(const sync_pb::SyncEnums::ErrorType& error_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (triggered_actionable_error_) {
    DVLOG(1) << "Only one type of error can be triggered at any given time.";
    return false;
  }

  error_type_ = error_type;
  return true;
}

bool FakeServer::TriggerActionableError(
    const sync_pb::SyncEnums::ErrorType& error_type,
    const std::string& description,
    const std::string& url,
    const sync_pb::SyncEnums::Action& action) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (error_type_ != sync_pb::SyncEnums::SUCCESS) {
    DVLOG(1) << "Only one type of error can be triggered at any given time.";
    return false;
  }

  sync_pb::ClientToServerResponse_Error* error =
      new sync_pb::ClientToServerResponse_Error();
  error->set_error_type(error_type);
  error->set_error_description(description);
  error->set_url(url);
  error->set_action(action);
  triggered_actionable_error_.reset(error);
  return true;
}

void FakeServer::ClearActionableError() {
  triggered_actionable_error_.reset();
}

bool FakeServer::EnableAlternatingTriggeredErrors() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (error_type_ == sync_pb::SyncEnums::SUCCESS &&
      !triggered_actionable_error_) {
    DVLOG(1) << "No triggered error set. Alternating can't be enabled.";
    return false;
  }

  alternate_triggered_errors_ = true;
  // Reset the counter so that the the first request yields a triggered error.
  request_counter_ = 0;
  return true;
}

bool FakeServer::ShouldSendTriggeredError() const {
  if (!alternate_triggered_errors_)
    return true;

  // Check that the counter is odd so that we trigger an error on the first
  // request after alternating is enabled.
  return request_counter_ % 2 != 0;
}

void FakeServer::AddObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.AddObserver(observer);
}

void FakeServer::RemoveObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void FakeServer::OnCommit(const std::string& committer_id,
                          syncer::ModelTypeSet committed_model_types) {
  for (auto& observer : observers_)
    observer.OnCommit(committer_id, committed_model_types);
}

void FakeServer::OnHistoryCommit(const std::string& url) {
  committed_history_urls_.insert(url);
}

void FakeServer::EnableStrongConsistencyWithConflictDetectionModel() {
  DCHECK(thread_checker_.CalledOnValidThread());
  loopback_server_->EnableStrongConsistencyWithConflictDetectionModel();
}

void FakeServer::SetMaxGetUpdatesBatchSize(int batch_size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  loopback_server_->SetMaxGetUpdatesBatchSize(batch_size);
}

void FakeServer::SetBagOfChips(const sync_pb::ChipBag& bag_of_chips) {
  DCHECK(thread_checker_.CalledOnValidThread());
  loopback_server_->SetBagOfChipsForTesting(bag_of_chips);
}

void FakeServer::TriggerMigrationDoneError(syncer::ModelTypeSet types) {
  DCHECK(thread_checker_.CalledOnValidThread());
  loopback_server_->TriggerMigrationForTesting(types);
}

const std::set<std::string>& FakeServer::GetCommittedHistoryURLs() const {
  return committed_history_urls_;
}

base::WeakPtr<FakeServer> FakeServer::AsWeakPtr() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace fake_server
