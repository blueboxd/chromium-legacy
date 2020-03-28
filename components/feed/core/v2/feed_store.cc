// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_store.h"
#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace feed {
namespace {

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
    default:
      return "";
  }
}

bool FilterByKey(const base::flat_set<std::string>& key_set,
                 const std::string& key) {
  return key_set.contains(key);
}

}  // namespace

FeedStore::FeedStore(leveldb_proto::ProtoDatabaseProvider* database_provider,
                     const base::FilePath& feed_directory,
                     scoped_refptr<base::SequencedTaskRunner> task_runner)
    : database_status_(leveldb_proto::Enums::InitStatus::kNotInitialized),
      database_(database_provider->GetDB<feedstore::Record>(
          leveldb_proto::ProtoDbType::FEED_STREAM_DATABASE,
          feed_directory,
          task_runner)) {
  Initialize();
}

FeedStore::FeedStore(
    std::unique_ptr<leveldb_proto::ProtoDatabase<feedstore::Record>> database)
    : database_status_(leveldb_proto::Enums::InitStatus::kNotInitialized),
      database_(std::move(database)) {
  Initialize();
}

FeedStore::~FeedStore() = default;

void FeedStore::Initialize() {
  database_->Init(base::BindOnce(&FeedStore::OnDatabaseInitialized,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void FeedStore::OnDatabaseInitialized(leveldb_proto::Enums::InitStatus status) {
  database_status_ = status;
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

void FeedStore::ReadStreamData(
    base::OnceCallback<void(std::unique_ptr<feedstore::StreamData>)>
        stream_data_callback) {
  ReadSingle(kStreamDataKey,
             base::BindOnce(&FeedStore::OnReadStreamDataFinished,
                            weak_ptr_factory_.GetWeakPtr(),
                            std::move(stream_data_callback)));
}

void FeedStore::OnReadStreamDataFinished(
    base::OnceCallback<void(std::unique_ptr<feedstore::StreamData>)> callback,
    bool success,
    std::unique_ptr<feedstore::Record> record) {
  if (!success || !record) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(base::WrapUnique(record->release_stream_data()));
}

void FeedStore::ReadContent(
    std::vector<feedwire::ContentId> content_ids,
    std::vector<feedwire::ContentId> shared_state_ids,
    base::OnceCallback<void(std::vector<feedstore::Content>,
                            std::vector<feedstore::StreamSharedState>)>
        content_callback) {
  std::vector<std::string> key_vector;
  key_vector.reserve(content_ids.size() + shared_state_ids.size());
  for (auto& content_id : content_ids)
    key_vector.push_back(ContentKey(content_id));

  for (auto& shared_state_id : shared_state_ids)
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
