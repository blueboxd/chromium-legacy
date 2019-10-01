// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_LEVELDB_LEVELDB_DATABASE_IMPL_H_
#define COMPONENTS_SERVICES_LEVELDB_LEVELDB_DATABASE_IMPL_H_

#include <memory>
#include <tuple>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/unguessable_token.h"
#include "components/services/leveldb/public/mojom/leveldb.mojom.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "third_party/leveldatabase/src/include/leveldb/cache.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace leveldb {

// A temporary wrapper around the Storage Service's DomStorageDatabase class,
// implemented behind the mojom interface facade consumed by
// LocalStorageContextMojo, SessionStorageContextMojo, and related classes. Note
// that this is never used through an actual message pipe, but always via direct
// method calls.
//
// TODO(https://crbug.com/1000959): Delete this class.
class LevelDBDatabaseImpl : public mojom::LevelDBDatabase {
 public:
  using OpenCallback = base::OnceCallback<void(mojom::DatabaseError)>;

  ~LevelDBDatabaseImpl() override;

  static std::unique_ptr<LevelDBDatabaseImpl> OpenDirectory(
      const leveldb_env::Options& options,
      const base::FilePath& directory,
      const std::string& dbname,
      const base::Optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      OpenCallback callback);

  static std::unique_ptr<LevelDBDatabaseImpl> OpenInMemory(
      const base::Optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      const std::string& tracking_name,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      OpenCallback callback);

  base::SequenceBound<storage::DomStorageDatabase>& database() {
    return database_;
  }
  const base::SequenceBound<storage::DomStorageDatabase>& database() const {
    return database_;
  }

  // Overridden from LevelDBDatabase:
  void Put(const std::vector<uint8_t>& key,
           const std::vector<uint8_t>& value,
           PutCallback callback) override;
  void Delete(const std::vector<uint8_t>& key,
              DeleteCallback callback) override;
  void DeletePrefixed(const std::vector<uint8_t>& key_prefix,
                      DeletePrefixedCallback callback) override;
  void RewriteDB(RewriteDBCallback callback) override;
  void Write(std::vector<mojom::BatchedOperationPtr> operations,
             WriteCallback callback) override;
  void Get(const std::vector<uint8_t>& key, GetCallback callback) override;
  void GetPrefixed(const std::vector<uint8_t>& key_prefix,
                   GetPrefixedCallback callback) override;
  void GetMany(std::vector<mojom::GetManyRequestPtr> keys_or_prefixes,
               GetManyCallback callback) override;
  void CopyPrefixed(const std::vector<uint8_t>& source_key_prefix,
                    const std::vector<uint8_t>& destination_key_prefix,
                    CopyPrefixedCallback callback) override;

 private:
  using StatusAndKeyValues =
      std::tuple<Status, std::vector<mojom::KeyValuePtr>>;

  void OnDatabaseOpened(
      OpenCallback callback,
      base::SequenceBound<storage::DomStorageDatabase> database,
      leveldb::Status status);

  explicit LevelDBDatabaseImpl();

  template <typename ResultType>
  void RunDatabaseTask(
      base::OnceCallback<ResultType(const storage::DomStorageDatabase&)> task,
      base::OnceCallback<void(ResultType)> callback) {
    auto wrapped_task = base::BindOnce(
        [](base::OnceCallback<ResultType(const storage::DomStorageDatabase&)>
               task,
           base::OnceCallback<void(ResultType)> callback,
           scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
           const storage::DomStorageDatabase& db) {
          callback_task_runner->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(callback), std::move(task).Run(db)));
        },
        std::move(task), std::move(callback),
        base::SequencedTaskRunnerHandle::Get());
    if (database_) {
      database_.PostTaskWithThisObject(FROM_HERE, std::move(wrapped_task));
    } else {
      tasks_to_run_on_open_.push_back(std::move(wrapped_task));
    }
  }

  base::SequenceBound<storage::DomStorageDatabase> database_;

  using DatabaseTask =
      base::OnceCallback<void(const storage::DomStorageDatabase&)>;
  std::vector<DatabaseTask> tasks_to_run_on_open_;

  base::WeakPtrFactory<LevelDBDatabaseImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LevelDBDatabaseImpl);
};

}  // namespace leveldb

#endif  // COMPONENTS_SERVICES_LEVELDB_LEVELDB_DATABASE_IMPL_H_
