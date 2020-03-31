// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_FEED_STORE_H_
#define COMPONENTS_FEED_CORE_V2_FEED_STORE_H_

#include <memory>
#include <vector>
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace feed {

class FeedStore {
 public:
  FeedStore(leveldb_proto::ProtoDatabaseProvider* database_provider,
            const base::FilePath& feed_directory,
            scoped_refptr<base::SequencedTaskRunner> task_runner);
  FeedStore(const FeedStore&) = delete;
  FeedStore& operator=(const FeedStore&) = delete;
  ~FeedStore();

  // For testing.
  explicit FeedStore(
      std::unique_ptr<leveldb_proto::ProtoDatabase<feedstore::Record>>
          database);

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

  void Write(std::vector<feedstore::Record> records,
             base::OnceCallback<void(bool)> callback);

  // TODO(iwells): implement this
  // Deletes old records that are no longer needed
  // void RemoveOldData(base::OnceCallback<void(bool)> callback);

  bool IsInitializedForTesting() const;

 private:
  void Initialize();
  void OnDatabaseInitialized(leveldb_proto::Enums::InitStatus status);
  bool IsInitialized() const;

  void ReadSingle(
      const std::string& key,
      base::OnceCallback<void(bool, std::unique_ptr<feedstore::Record>)>
          callback);
  void ReadMany(const base::flat_set<std::string>& key_set,
                base::OnceCallback<
                    void(bool, std::unique_ptr<std::vector<feedstore::Record>>)>
                    callback);

  void OnReadStreamDataFinished(
      base::OnceCallback<void(std::unique_ptr<feedstore::StreamData>)> callback,
      bool success,
      std::unique_ptr<feedstore::Record> record);
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

  leveldb_proto::Enums::InitStatus database_status_;
  std::unique_ptr<leveldb_proto::ProtoDatabase<feedstore::Record>> database_;

  base::WeakPtrFactory<FeedStore> weak_ptr_factory_{this};
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_FEED_STORE_H_
