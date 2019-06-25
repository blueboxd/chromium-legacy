// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_LEVELDB_OPERATIONS_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_LEVELDB_OPERATIONS_H_

#include <memory>
#include <string>
#include <tuple>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "content/browser/indexed_db/leveldb/leveldb_comparator.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"
#include "third_party/leveldatabase/src/include/leveldb/comparator.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "url/origin.h"

// Contains common operations for LevelDBTransactions and/or LevelDBDatabases.

namespace content {
class LevelDBDatabase;
class LevelDBIterator;
class LevelDBTransaction;
struct IndexedDBDataLossInfo;

namespace indexed_db {
class LevelDBFactory;

extern const base::FilePath::CharType kBlobExtension[];
extern const base::FilePath::CharType kIndexedDBExtension[];
extern const base::FilePath::CharType kLevelDBExtension[];

base::FilePath GetBlobStoreFileName(const url::Origin& origin);
base::FilePath GetLevelDBFileName(const url::Origin& origin);
base::FilePath ComputeCorruptionFileName(const url::Origin& origin);

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

leveldb::Status CONTENT_EXPORT GetInt(LevelDBDatabase* db,
                                      const base::StringPiece& key,
                                      int64_t* found_int,
                                      bool* found);
leveldb::Status CONTENT_EXPORT GetInt(LevelDBTransaction* txn,
                                      const base::StringPiece& key,
                                      int64_t* found_int,
                                      bool* found);

void PutBool(LevelDBTransaction* transaction,
             const base::StringPiece& key,
             bool value);
void CONTENT_EXPORT PutInt(LevelDBTransaction* transaction,
                           const base::StringPiece& key,
                           int64_t value);

template <typename DBOrTransaction>
WARN_UNUSED_RESULT leveldb::Status GetVarInt(DBOrTransaction* db,
                                             const base::StringPiece& key,
                                             int64_t* found_int,
                                             bool* found);

void PutVarInt(LevelDBTransaction* transaction,
               const base::StringPiece& key,
               int64_t value);

template <typename DBOrTransaction>
WARN_UNUSED_RESULT leveldb::Status GetString(DBOrTransaction* db,
                                             const base::StringPiece& key,
                                             base::string16* found_string,
                                             bool* found);

void PutString(LevelDBTransaction* transaction,
               const base::StringPiece& key,
               const base::string16& value);

void PutIDBKeyPath(LevelDBTransaction* transaction,
                   const base::StringPiece& key,
                   const blink::IndexedDBKeyPath& value);

template <typename DBOrTransaction>
WARN_UNUSED_RESULT leveldb::Status GetMaxObjectStoreId(
    DBOrTransaction* db,
    int64_t database_id,
    int64_t* max_object_store_id);

WARN_UNUSED_RESULT leveldb::Status SetMaxObjectStoreId(
    LevelDBTransaction* transaction,
    int64_t database_id,
    int64_t object_store_id);

WARN_UNUSED_RESULT leveldb::Status GetNewVersionNumber(
    LevelDBTransaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t* new_version_number);

WARN_UNUSED_RESULT leveldb::Status SetMaxIndexId(
    LevelDBTransaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id);

WARN_UNUSED_RESULT leveldb::Status VersionExists(
    LevelDBTransaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t version,
    const std::string& encoded_primary_key,
    bool* exists);

WARN_UNUSED_RESULT leveldb::Status GetNewDatabaseId(
    LevelDBTransaction* transaction,
    int64_t* new_id);

WARN_UNUSED_RESULT bool CheckObjectStoreAndMetaDataType(
    const LevelDBIterator* it,
    const std::string& stop_key,
    int64_t object_store_id,
    int64_t meta_data_type);

WARN_UNUSED_RESULT bool CheckIndexAndMetaDataKey(const LevelDBIterator* it,
                                                 const std::string& stop_key,
                                                 int64_t index_id,
                                                 unsigned char meta_data_type);

WARN_UNUSED_RESULT bool FindGreatestKeyLessThanOrEqual(
    LevelDBTransaction* transaction,
    const std::string& target,
    std::string* found_key,
    leveldb::Status* s);

WARN_UNUSED_RESULT bool GetBlobKeyGeneratorCurrentNumber(
    LevelDBTransaction* leveldb_transaction,
    int64_t database_id,
    int64_t* blob_key_generator_current_number);

WARN_UNUSED_RESULT bool UpdateBlobKeyGeneratorCurrentNumber(
    LevelDBTransaction* leveldb_transaction,
    int64_t database_id,
    int64_t blob_key_generator_current_number);

WARN_UNUSED_RESULT leveldb::Status GetEarliestSweepTime(
    LevelDBDatabase* db,
    base::Time* earliest_sweep);

void SetEarliestSweepTime(LevelDBTransaction* txn, base::Time earliest_sweep);

CONTENT_EXPORT const LevelDBComparator* GetDefaultIndexedDBComparator();

CONTENT_EXPORT const leveldb::Comparator* GetDefaultLevelDBComparator();

// Creates the leveldb and blob storage directories for IndexedDB.
std::tuple<base::FilePath /*leveldb_path*/,
           base::FilePath /*blob_path*/,
           leveldb::Status>
CreateDatabaseDirectories(const base::FilePath& path_base,
                          const url::Origin& origin);

// |path_base| is the directory that will contain the database directory, the
// blob directory, and any data loss info. |database_path| is the directory for
// the leveldb database. If |path_base| is empty, then an in-memory database is
// opened.
std::tuple<std::unique_ptr<LevelDBDatabase>,
           leveldb::Status,
           IndexedDBDataLossInfo,
           bool /* is_disk_full */>
OpenAndVerifyLevelDBDatabase(
    const url::Origin& origin,
    base::FilePath path_base,
    base::FilePath database_path,
    LevelDBFactory* ldb_factory,
    scoped_refptr<base::SequencedTaskRunner> task_runner);

}  // namespace indexed_db
}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_LEVELDB_OPERATIONS_H_
