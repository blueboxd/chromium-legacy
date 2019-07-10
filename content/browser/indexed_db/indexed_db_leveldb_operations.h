// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_LEVELDB_OPERATIONS_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_LEVELDB_OPERATIONS_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "content/browser/indexed_db/indexed_db_data_loss_info.h"
#include "content/browser/indexed_db/leveldb/leveldb_comparator.h"
#include "content/browser/indexed_db/scopes/leveldb_scopes_factory.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"
#include "third_party/leveldatabase/src/include/leveldb/comparator.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "url/origin.h"

// Contains common operations for LevelDBTransactions and/or LevelDBDatabases.

namespace content {
class TransactionalLevelDBDatabase;
class TransactionalLevelDBIterator;
class TransactionalLevelDBTransaction;

namespace indexed_db {

extern const base::FilePath::CharType kBlobExtension[];
extern const base::FilePath::CharType kIndexedDBExtension[];
extern const base::FilePath::CharType kLevelDBExtension[];

base::FilePath GetBlobStoreFileName(const url::Origin& origin);
base::FilePath GetLevelDBFileName(const url::Origin& origin);
base::FilePath ComputeCorruptionFileName(const url::Origin& origin);

// Returns if the given file path is too long for the current operating system's
// file system.
bool IsPathTooLong(const base::FilePath& leveldb_dir);

// If a corruption file for the given |origin| at the given |path_base| exists
// it is deleted, and the message is returned. If the file does not exist, or if
// there is an error parsing the message, then this method returns an empty
// string (and deletes the file).
std::string CONTENT_EXPORT ReadCorruptionInfo(const base::FilePath& path_base,
                                              const url::Origin& origin);

// Was able to use LevelDB to read the data w/o error, but the data read was not
// in the expected format.
leveldb::Status InternalInconsistencyStatus();

leveldb::Status InvalidDBKeyStatus();

leveldb::Status IOErrorStatus();

template <typename DBOrTransaction>
leveldb::Status CONTENT_EXPORT GetInt(DBOrTransaction* db,
                                      const base::StringPiece& key,
                                      int64_t* found_int,
                                      bool* found);

void PutBool(TransactionalLevelDBTransaction* transaction,
             const base::StringPiece& key,
             bool value);

template <typename TransactionOrWriteBatch>
void CONTENT_EXPORT PutInt(TransactionOrWriteBatch* transaction,
                           const base::StringPiece& key,
                           int64_t value);

template <typename DBOrTransaction>
WARN_UNUSED_RESULT leveldb::Status GetVarInt(DBOrTransaction* db,
                                             const base::StringPiece& key,
                                             int64_t* found_int,
                                             bool* found);

template <typename TransactionOrWriteBatch>
void PutVarInt(TransactionOrWriteBatch* transaction,
               const base::StringPiece& key,
               int64_t value);

template <typename DBOrTransaction>
WARN_UNUSED_RESULT leveldb::Status GetString(DBOrTransaction* db,
                                             const base::StringPiece& key,
                                             base::string16* found_string,
                                             bool* found);

void PutString(TransactionalLevelDBTransaction* transaction,
               const base::StringPiece& key,
               const base::string16& value);

void PutIDBKeyPath(TransactionalLevelDBTransaction* transaction,
                   const base::StringPiece& key,
                   const blink::IndexedDBKeyPath& value);

template <typename DBOrTransaction>
WARN_UNUSED_RESULT leveldb::Status GetMaxObjectStoreId(
    DBOrTransaction* db,
    int64_t database_id,
    int64_t* max_object_store_id);

WARN_UNUSED_RESULT leveldb::Status SetMaxObjectStoreId(
    TransactionalLevelDBTransaction* transaction,
    int64_t database_id,
    int64_t object_store_id);

WARN_UNUSED_RESULT leveldb::Status GetNewVersionNumber(
    TransactionalLevelDBTransaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t* new_version_number);

WARN_UNUSED_RESULT leveldb::Status SetMaxIndexId(
    TransactionalLevelDBTransaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id);

WARN_UNUSED_RESULT leveldb::Status VersionExists(
    TransactionalLevelDBTransaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t version,
    const std::string& encoded_primary_key,
    bool* exists);

template <typename Transaction>
WARN_UNUSED_RESULT leveldb::Status GetNewDatabaseId(Transaction* transaction,
                                                    int64_t* new_id);

WARN_UNUSED_RESULT bool CheckObjectStoreAndMetaDataType(
    const TransactionalLevelDBIterator* it,
    const std::string& stop_key,
    int64_t object_store_id,
    int64_t meta_data_type);

WARN_UNUSED_RESULT bool CheckIndexAndMetaDataKey(
    const TransactionalLevelDBIterator* it,
    const std::string& stop_key,
    int64_t index_id,
    unsigned char meta_data_type);

WARN_UNUSED_RESULT bool FindGreatestKeyLessThanOrEqual(
    TransactionalLevelDBTransaction* transaction,
    const std::string& target,
    std::string* found_key,
    leveldb::Status* s);

WARN_UNUSED_RESULT bool GetBlobKeyGeneratorCurrentNumber(
    TransactionalLevelDBTransaction* leveldb_transaction,
    int64_t database_id,
    int64_t* blob_key_generator_current_number);

WARN_UNUSED_RESULT bool UpdateBlobKeyGeneratorCurrentNumber(
    TransactionalLevelDBTransaction* leveldb_transaction,
    int64_t database_id,
    int64_t blob_key_generator_current_number);

WARN_UNUSED_RESULT leveldb::Status GetEarliestSweepTime(
    TransactionalLevelDBDatabase* db,
    base::Time* earliest_sweep);

template <typename Transaction>
void SetEarliestSweepTime(Transaction* txn, base::Time earliest_sweep);

CONTENT_EXPORT const LevelDBComparator* GetDefaultIndexedDBComparator();

CONTENT_EXPORT const leveldb::Comparator* GetDefaultLevelDBComparator();

}  // namespace indexed_db
}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_LEVELDB_OPERATIONS_H_
