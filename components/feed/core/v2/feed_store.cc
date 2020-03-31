// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_store.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "components/feed/core/v2/stream_model_update_request.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace feed {
namespace {

// Keys are defined as:
// 'S/<stream-id>' -> stream_data
// 'T/<stream-id>/<sequence-number>' -> stream_structures
// 'c/<content-id>' -> content
// 'a/<id>' -> action
// 's/<content-id>' -> shared_state
// 'N' -> next_stream_state
constexpr char kMainStreamId[] = "0";
const char kStreamDataKey[] = "S/0";
const char kLocalActionPrefix[] = "a/";
const char kNextStreamStateKey[] = "N";

leveldb::ReadOptions CreateReadOptions() {
  leveldb::ReadOptions opts;
  opts.fill_cache = false;
  return opts;
}

std::string KeyForContentId(base::StringPiece prefix,
                            const feedwire::ContentId& content_id) {
  return base::StrCat({prefix, content_id.content_domain(), ",",
                       base::NumberToString(content_id.type()), ",",
                       base::NumberToString(content_id.id())});
}

std::string ContentKey(const feedwire::ContentId& content_id) {
  return KeyForContentId("c/", content_id);
}

std::string SharedStateKey(const feedwire::ContentId& content_id) {
  return KeyForContentId("s/", content_id);
}

std::string KeyForRecord(const feedstore::Record& record) {
  switch (record.data_case()) {
    case feedstore::Record::kStreamData:
      return kStreamDataKey;
    case feedstore::Record::kStreamStructures:
      return base::StrCat(
          {"T/", record.stream_structures().stream_id(), "/",
           base::NumberToString(record.stream_structures().sequence_number())});
    case feedstore::Record::kContent:
      return ContentKey(record.content().content_id());
    case feedstore::Record::kLocalAction:
      return kLocalActionPrefix +
             base::NumberToString(record.local_action().id());
    case feedstore::Record::kSharedState:
      return SharedStateKey(record.shared_state().content_id());
    case feedstore::Record::kNextStreamState:
      return kNextStreamStateKey;
    case feedstore::Record::DATA_NOT_SET:  // fall through
      NOTREACHED() << "Invalid record case " << record.data_case();
      return "";
  }
}

bool FilterByKey(const base::flat_set<std::string>& key_set,
                 const std::string& key) {
  return key_set.contains(key);
}

feedstore::Record MakeRecord(feedstore::Content content) {
  feedstore::Record record;
  *record.mutable_content() = std::move(content);
  return record;
}

feedstore::Record MakeRecord(
    feedstore::StreamStructureSet stream_structure_set) {
  feedstore::Record record;
  *record.mutable_stream_structures() = std::move(stream_structure_set);
  return record;
}

feedstore::Record MakeRecord(feedstore::StreamSharedState shared_state) {
  feedstore::Record record;
  *record.mutable_shared_state() = std::move(shared_state);
  return record;
}

feedstore::Record MakeRecord(feedstore::StreamData stream_data) {
  feedstore::Record record;
  *record.mutable_stream_data() = std::move(stream_data);
  return record;
}

template <typename T>
std::pair<std::string, feedstore::Record> MakeKeyAndRecord(T record_data) {
  std::pair<std::string, feedstore::Record> result;
  result.second = MakeRecord(std::move(record_data));
  result.first = KeyForRecord(result.second);
  return result;
}

}  // namespace

FeedStore::LoadStreamResult::LoadStreamResult() = default;
FeedStore::LoadStreamResult::~LoadStreamResult() = default;
FeedStore::LoadStreamResult::LoadStreamResult(LoadStreamResult&&) = default;
FeedStore::LoadStreamResult& FeedStore::LoadStreamResult::operator=(
    LoadStreamResult&&) = default;

FeedStore::FeedStore(
    std::unique_ptr<leveldb_proto::ProtoDatabase<feedstore::Record>> database)
    : database_status_(leveldb_proto::Enums::InitStatus::kNotInitialized),
      database_(std::move(database)) {
}

FeedStore::~FeedStore() = default;

void FeedStore::Initialize(base::OnceClosure initialize_complete) {
  if (IsInitialized()) {
    std::move(initialize_complete).Run();
  } else {
    initialize_callback_ = std::move(initialize_complete);
    database_->Init(base::BindOnce(&FeedStore::OnDatabaseInitialized,
                                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void FeedStore::OnDatabaseInitialized(leveldb_proto::Enums::InitStatus status) {
  database_status_ = status;
  if (initialize_callback_)
    std::move(initialize_callback_).Run();
}

bool FeedStore::IsInitialized() const {
  return database_status_ == leveldb_proto::Enums::InitStatus::kOK;
}

bool FeedStore::IsInitializedForTesting() const {
  return IsInitialized();
}

void FeedStore::ReadSingle(
    const std::string& key,
    base::OnceCallback<void(bool, std::unique_ptr<feedstore::Record>)>
        callback) {
  if (!IsInitialized()) {
    std::move(callback).Run(false, nullptr);
    return;
  }

  database_->GetEntry(key, std::move(callback));
}

void FeedStore::ReadMany(
    const base::flat_set<std::string>& key_set,
    base::OnceCallback<
        void(bool, std::unique_ptr<std::vector<feedstore::Record>>)> callback) {
  if (!IsInitialized()) {
    std::move(callback).Run(false, nullptr);
    return;
  }

  database_->LoadEntriesWithFilter(
      base::BindRepeating(&FilterByKey, std::move(key_set)),
      CreateReadOptions(),
      /*target_prefix=*/"", std::move(callback));
}

void FeedStore::LoadStream(
    base::OnceCallback<void(LoadStreamResult)> callback) {
  if (!IsInitialized()) {
    LoadStreamResult result;
    result.read_error = true;
    std::move(callback).Run(std::move(result));
    return;
  }
  auto filter = [](const std::string& key) {
    return key == "S/0" || (key.size() > 3 && key[0] == 'T' && key[1] == '/' &&
                            key[2] == '0' && key[3] == '/');
  };
  database_->LoadEntriesWithFilter(
      base::BindRepeating(filter), CreateReadOptions(),
      /*target_prefix=*/"",
      base::BindOnce(&FeedStore::OnLoadStreamFinished, base::Unretained(this),
                     std::move(callback)));
}

void FeedStore::OnLoadStreamFinished(
    base::OnceCallback<void(LoadStreamResult)> callback,
    bool success,
    std::unique_ptr<std::vector<feedstore::Record>> records) {
  LoadStreamResult result;
  if (!records || !success) {
    result.read_error = true;
  } else {
    for (feedstore::Record& record : *records) {
      if (record.has_stream_structures()) {
        result.stream_structures.push_back(
            std::move(*record.mutable_stream_structures()));
      } else if (record.has_stream_data()) {
        result.stream_data = std::move(*record.mutable_stream_data());
      }
    }
  }
  std::move(callback).Run(std::move(result));
}

void FeedStore::SaveFullStream(
    std::unique_ptr<StreamModelUpdateRequest> update_request,
    base::OnceCallback<void(bool)> callback) {
  auto updates = std::make_unique<
      std::vector<std::pair<std::string, feedstore::Record>>>();
  updates->push_back(MakeKeyAndRecord(std::move(update_request->stream_data)));
  for (feedstore::Content& content : update_request->content) {
    updates->push_back(MakeKeyAndRecord(std::move(content)));
  }
  for (feedstore::StreamSharedState& shared_state :
       update_request->shared_states) {
    updates->push_back(MakeKeyAndRecord(std::move(shared_state)));
  }
  feedstore::StreamStructureSet stream_structure_set;
  stream_structure_set.set_stream_id(kMainStreamId);
  for (feedstore::StreamStructure& structure :
       update_request->stream_structures) {
    *stream_structure_set.add_structures() = std::move(structure);
  }
  updates->push_back(MakeKeyAndRecord(std::move(stream_structure_set)));

  // Set up a filter to delete all stream-related data.
  // But we need to exclude keys being written right now.
  std::vector<std::string> key_vector(updates->size());
  for (size_t i = 0; i < key_vector.size(); ++i) {
    key_vector[i] = (*updates)[i].first;
  }
  base::flat_set<std::string> updated_keys(std::move(key_vector));

  auto filter = [](const base::flat_set<std::string>& updated_keys,
                   const std::string& key) {
    if (key.empty() || updated_keys.contains(key))
      return false;
    return key[0] == 'S' || key[0] == 'T' || key[0] == 'c' || key[0] == 's' ||
           key[0] == 'N';
  };

  database_->UpdateEntriesWithRemoveFilter(
      std::move(updates), base::BindRepeating(filter, std::move(updated_keys)),
      base::BindOnce(&FeedStore::OnSaveStreamEntriesUpdated,
                     base::Unretained(this), std::move(callback)));
}

void FeedStore::OnSaveStreamEntriesUpdated(
    base::OnceCallback<void(bool)> complete_callback,
    bool ok) {
  std::move(complete_callback).Run(ok);
}

void FeedStore::WriteOperations(
    int32_t sequence_number,
    std::vector<feedstore::DataOperation> operations) {
  std::vector<feedstore::Record> records;
  feedstore::Record structures_record;
  feedstore::StreamStructureSet& structure_set =
      *structures_record.mutable_stream_structures();
  for (feedstore::DataOperation& operation : operations) {
    *structure_set.add_structures() = std::move(*operation.mutable_structure());
    if (operation.has_content()) {
      feedstore::Record record;
      record.set_allocated_content(operation.release_content());
      records.push_back(std::move(record));
    }
  }
  structure_set.set_stream_id(kMainStreamId);
  structure_set.set_sequence_number(sequence_number);

  records.push_back(std::move(structures_record));
  Write(std::move(records), base::DoNothing());
}

void FeedStore::ReadContent(
    std::vector<feedwire::ContentId> content_ids,
    std::vector<feedwire::ContentId> shared_state_ids,
    base::OnceCallback<void(std::vector<feedstore::Content>,
                            std::vector<feedstore::StreamSharedState>)>
        content_callback) {
  std::vector<std::string> key_vector;
  key_vector.reserve(content_ids.size() + shared_state_ids.size());
  for (const auto& content_id : content_ids)
    key_vector.push_back(ContentKey(content_id));
  for (const auto& content_id : shared_state_ids)
    key_vector.push_back(SharedStateKey(content_id));

  for (const auto& shared_state_id : shared_state_ids)
    key_vector.push_back(SharedStateKey(shared_state_id));

  ReadMany(base::flat_set<std::string>(std::move(key_vector)),
           base::BindOnce(&FeedStore::OnReadContentFinished,
                          weak_ptr_factory_.GetWeakPtr(),
                          std::move(content_callback)));
}

void FeedStore::OnReadContentFinished(
    base::OnceCallback<void(std::vector<feedstore::Content>,
                            std::vector<feedstore::StreamSharedState>)>
        callback,
    bool success,
    std::unique_ptr<std::vector<feedstore::Record>> records) {
  if (!success || !records) {
    std::move(callback).Run({}, {});
    return;
  }

  std::vector<feedstore::Content> content;
  // Most of records will be content.
  content.reserve(records->size());
  std::vector<feedstore::StreamSharedState> shared_states;
  for (auto& record : *records) {
    if (record.data_case() == feedstore::Record::kContent)
      content.push_back(std::move(record.content()));
    else if (record.data_case() == feedstore::Record::kSharedState)
      shared_states.push_back(std::move(record.shared_state()));
  }

  std::move(callback).Run(std::move(content), std::move(shared_states));
}

void FeedStore::ReadNextStreamState(
    base::OnceCallback<void(std::unique_ptr<feedstore::StreamAndContentState>)>
        callback) {
  ReadSingle(
      kNextStreamStateKey,
      base::BindOnce(&FeedStore::OnReadNextStreamStateFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FeedStore::OnReadNextStreamStateFinished(
    base::OnceCallback<void(std::unique_ptr<feedstore::StreamAndContentState>)>
        callback,
    bool success,
    std::unique_ptr<feedstore::Record> record) {
  if (!success || !record) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(
      base::WrapUnique(record->release_next_stream_state()));
}

void FeedStore::Write(std::vector<feedstore::Record> records,
                      base::OnceCallback<void(bool)> callback) {
  auto entries_to_save = std::make_unique<
      leveldb_proto::ProtoDatabase<feedstore::Record>::KeyEntryVector>();
  for (auto& record : records) {
    std::string key = KeyForRecord(record);
    if (!key.empty())
      entries_to_save->push_back({std::move(key), std::move(record)});
  }

  database_->UpdateEntries(
      std::move(entries_to_save),
      /*keys_to_remove=*/std::make_unique<leveldb_proto::KeyVector>(),
      base::BindOnce(&FeedStore::OnWriteFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FeedStore::OnWriteFinished(base::OnceCallback<void(bool)> callback,
                                bool success) {
  std::move(callback).Run(success);
}

}  // namespace feed
