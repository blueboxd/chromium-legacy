// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_FEED_STORE_H_
#define COMPONENTS_FEED_CORE_V2_FEED_STORE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace feed {
struct StreamModelUpdateRequest;

class FeedStore {
 public:
  struct LoadStreamResult {
    LoadStreamResult();
    ~LoadStreamResult();
    LoadStreamResult(LoadStreamResult&&);
    LoadStreamResult& operator=(LoadStreamResult&&);
    LoadStreamResult(const LoadStreamResult&) = delete;
    LoadStreamResult& operator=(const LoadStreamResult&) = delete;

    bool read_error = false;
    feedstore::StreamData stream_data;
    std::vector<feedstore::StreamStructureSet> stream_structures;
  };

  explicit FeedStore(
      std::unique_ptr<leveldb_proto::ProtoDatabase<feedstore::Record>>
          database);
  ~FeedStore();
  FeedStore(const FeedStore&) = delete;
  FeedStore& operator=(const FeedStore&) = delete;

  void Initialize(base::OnceClosure initialize_complete);

  void LoadStream(base::OnceCallback<void(LoadStreamResult)> callback);

  void SaveFullStream(std::unique_ptr<StreamModelUpdateRequest> update_request,
                      base::OnceCallback<void(bool)> callback);

  void WriteOperations(int32_t sequence_number,
                       std::vector<feedstore::DataOperation> operations);

  // Read StreamData and pass it to stream_data_callback, or nullptr on failure.
  void ReadStreamData(
      base::OnceCallback<void(std::unique_ptr<feedstore::StreamData>)>
          stream_data_callback);

  // Read Content and StreamSharedStates and pass them to content_callback, or
  // nullptrs on failure.
  void ReadContent(
      std::vector<feedwire::ContentId> content_ids,
      std::vector<feedwire::ContentId> shared_state_ids,
      base::OnceCallback<void(std::vector<feedstore::Content>,
                              std::vector<feedstore::StreamSharedState>)>
          content_callback);

  void ReadNextStreamState(
      base::OnceCallback<
          void(std::unique_ptr<feedstore::StreamAndContentState>)> callback);

  // TODO(iwells): implement reading stored actions

  // TODO(iwells): implement this
  // Deletes old records that are no longer needed
  // void RemoveOldData(base::OnceCallback<void(bool)> callback);

  bool IsInitializedForTesting() const;

 private:
  void OnDatabaseInitialized(leveldb_proto::Enums::InitStatus status);
  bool IsInitialized() const;

  void Write(std::vector<feedstore::Record> records,
             base::OnceCallback<void(bool)> callback);

  void ReadSingle(
      const std::string& key,
      base::OnceCallback<void(bool, std::unique_ptr<feedstore::Record>)>
          callback);
  void ReadMany(const base::flat_set<std::string>& key_set,
                base::OnceCallback<
                    void(bool, std::unique_ptr<std::vector<feedstore::Record>>)>
                    callback);
  void OnSaveStreamEntriesUpdated(
      base::OnceCallback<void(bool)> complete_callback,
      bool ok);
  void OnLoadStreamFinished(
      base::OnceCallback<void(LoadStreamResult)> callback,
      bool success,
      std::unique_ptr<std::vector<feedstore::Record>> records);
  void OnReadContentFinished(
      base::OnceCallback<void(std::vector<feedstore::Content>,
                              std::vector<feedstore::StreamSharedState>)>
          callback,
      bool success,
      std::unique_ptr<std::vector<feedstore::Record>> records);
  void OnReadNextStreamStateFinished(
      base::OnceCallback<
          void(std::unique_ptr<feedstore::StreamAndContentState>)> callback,
      bool success,
      std::unique_ptr<feedstore::Record> record);

  void OnWriteFinished(base::OnceCallback<void(bool)> callback, bool success);

  // TODO(iwells): implement
  // bool OldRecordFilter(const std::string& key);
  // void OnRemoveOldDataFinished(base::OnceCallback<void(bool)> callback,
  //                             bool success);

  base::OnceClosure initialize_callback_;
  leveldb_proto::Enums::InitStatus database_status_;
  std::unique_ptr<leveldb_proto::ProtoDatabase<feedstore::Record>> database_;
  base::WeakPtrFactory<FeedStore> weak_ptr_factory_{this};
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_FEED_STORE_H_
