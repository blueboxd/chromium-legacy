// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "content/browser/indexed_db/indexed_db_data_format_version.h"
#include "content/browser/indexed_db/indexed_db_data_loss_info.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_reporting.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/leveldb/leveldb_database.h"
#include "content/browser/indexed_db/leveldb/leveldb_env.h"
#include "content/browser/indexed_db/leveldb/leveldb_iterator.h"
#include "content/browser/indexed_db/leveldb/leveldb_transaction.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/leveldatabase/env_chromium.h"

using base::StringPiece;
using blink::IndexedDBKeyPath;
using leveldb::Status;

namespace content {
namespace indexed_db {
namespace {

class IDBComparator : public LevelDBComparator {
 public:
  IDBComparator() = default;
  ~IDBComparator() override = default;
  int Compare(const base::StringPiece& a,
              const base::StringPiece& b) const override {
    return content::Compare(a, b, false /*index_keys*/);
  }
  const char* Name() const override { return "idb_cmp1"; }
};

class LDBComparator : public leveldb::Comparator {
 public:
  LDBComparator() = default;
  ~LDBComparator() override = default;
  int Compare(const leveldb::Slice& a, const leveldb::Slice& b) const override {
    return content::Compare(leveldb_env::MakeStringPiece(a),
                            leveldb_env::MakeStringPiece(b),
                            false /*index_keys*/);
  }
  const char* Name() const override { return "idb_cmp1"; }
  void FindShortestSeparator(std::string* start,
                             const leveldb::Slice& limit) const override {}
  void FindShortSuccessor(std::string* key) const override {}
};

bool IsPathTooLong(const base::FilePath& leveldb_dir) {
  int limit = base::GetMaximumPathComponentLength(leveldb_dir.DirName());
  if (limit == -1) {
    DLOG(WARNING) << "GetMaximumPathComponentLength returned -1";
// In limited testing, ChromeOS returns 143, other OSes 255.
#if defined(OS_CHROMEOS)
    limit = 143;
#else
    limit = 255;
#endif
  }
  size_t component_length = leveldb_dir.BaseName().value().length();
  if (component_length > static_cast<uint32_t>(limit)) {
    DLOG(WARNING) << "Path component length (" << component_length
                  << ") exceeds maximum (" << limit
                  << ") allowed by this filesystem.";
    const int min = 140;
    const int max = 300;
    const int num_buckets = 12;
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "WebCore.IndexedDB.BackingStore.OverlyLargeOriginLength",
        component_length, min, max, num_buckets);
    return true;
  }
  return false;
}

template <typename DBOrTransaction>
Status GetIntInternal(DBOrTransaction* db,
                      const StringPiece& key,
                      int64_t* found_int,
                      bool* found) {
  std::string result;
  Status s = db->Get(key, &result, found);
  if (!s.ok())
    return s;
  if (!*found)
    return Status::OK();
  StringPiece slice(result);
  if (DecodeInt(&slice, found_int) && slice.empty())
    return s;
  return InternalInconsistencyStatus();
}

WARN_UNUSED_RESULT bool IsSchemaKnown(LevelDBDatabase* db, bool* known) {
  int64_t db_schema_version = 0;
  bool found = false;
  Status s = GetInt(db, SchemaVersionKey::Encode(), &db_schema_version, &found);
  if (!s.ok())
    return false;
  if (!found) {
    *known = true;
    return true;
  }
  if (db_schema_version < 0)
    return false;  // Only corruption should cause this.
  if (db_schema_version > indexed_db::kLatestKnownSchemaVersion) {
    *known = false;
    return true;
  }

  int64_t raw_db_data_version = 0;
  s = GetInt(db, DataVersionKey::Encode(), &raw_db_data_version, &found);
  if (!s.ok())
    return false;
  if (!found) {
    *known = true;
    return true;
  }
  if (raw_db_data_version < 0)
    return false;  // Only corruption should cause this.

  *known = IndexedDBDataFormatVersion::GetCurrent().IsAtLeast(
      IndexedDBDataFormatVersion::Decode(raw_db_data_version));
  return true;
}
std::tuple<std::unique_ptr<LevelDBDatabase>,
           leveldb::Status,
           bool /* is_disk_full */>
DeleteAndRecreateDatabase(
    const url::Origin& origin,
    base::FilePath database_path,
    LevelDBFactory* ldb_factory,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  scoped_refptr<LevelDBState> state;
  DCHECK(!database_path.empty())
      << "Opening an in-memory database should not have failed.";
  LOG(ERROR) << "IndexedDB backing store open failed, attempting cleanup";
  state.reset();
  leveldb::Status status = ldb_factory->DestroyLevelDB(database_path);
  if (!status.ok()) {
    LOG(ERROR) << "IndexedDB backing store cleanup failed";
    ReportOpenStatus(
        indexed_db::INDEXED_DB_BACKING_STORE_OPEN_CLEANUP_DESTROY_FAILED,
        origin);
    return {nullptr, status, false};
  }

  LOG(ERROR) << "IndexedDB backing store cleanup succeeded, reopening";
  state.reset();
  bool is_disk_full;
  std::tie(state, status, is_disk_full) = ldb_factory->OpenLevelDBState(
      database_path, GetDefaultIndexedDBComparator(),
      GetDefaultLevelDBComparator());
  if (!status.ok()) {
    LOG(ERROR) << "IndexedDB backing store reopen after recovery failed";
    ReportOpenStatus(
        indexed_db::INDEXED_DB_BACKING_STORE_OPEN_CLEANUP_REOPEN_FAILED,
        origin);
    return {nullptr, status, is_disk_full};
  }
  std::unique_ptr<LevelDBDatabase> database = std::make_unique<LevelDBDatabase>(
      std::move(state), ldb_factory, std::move(task_runner),
      LevelDBDatabase::kDefaultMaxOpenIteratorsPerDatabase);
  ReportOpenStatus(
      indexed_db::INDEXED_DB_BACKING_STORE_OPEN_CLEANUP_REOPEN_SUCCESS, origin);

  return {std::move(database), status, is_disk_full};
}

}  // namespace

const base::FilePath::CharType kBlobExtension[] = FILE_PATH_LITERAL(".blob");
const base::FilePath::CharType kIndexedDBExtension[] =
    FILE_PATH_LITERAL(".indexeddb");
const base::FilePath::CharType kLevelDBExtension[] =
    FILE_PATH_LITERAL(".leveldb");

// static
base::FilePath GetBlobStoreFileName(const url::Origin& origin) {
  std::string origin_id = storage::GetIdentifierFromOrigin(origin);
  return base::FilePath()
      .AppendASCII(origin_id)
      .AddExtension(kIndexedDBExtension)
      .AddExtension(kBlobExtension);
}

// static
base::FilePath GetLevelDBFileName(const url::Origin& origin) {
  std::string origin_id = storage::GetIdentifierFromOrigin(origin);
  return base::FilePath()
      .AppendASCII(origin_id)
      .AddExtension(kIndexedDBExtension)
      .AddExtension(kLevelDBExtension);
}

base::FilePath ComputeCorruptionFileName(const url::Origin& origin) {
  return GetLevelDBFileName(origin).Append(
      FILE_PATH_LITERAL("corruption_info.json"));
}

std::string ReadCorruptionInfo(const base::FilePath& path_base,
                               const url::Origin& origin) {
  const base::FilePath info_path =
      path_base.Append(ComputeCorruptionFileName(origin));
  std::string message;
  if (IsPathTooLong(info_path))
    return message;

  const int64_t kMaxJsonLength = 4096;
  int64_t file_size = 0;
  if (!base::GetFileSize(info_path, &file_size))
    return message;
  if (!file_size || file_size > kMaxJsonLength) {
    base::DeleteFile(info_path, false);
    return message;
  }

  base::File file(info_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (file.IsValid()) {
    std::string input_js(file_size, '\0');
    if (file_size == file.Read(0, base::data(input_js), file_size)) {
      base::JSONReader reader;
      std::unique_ptr<base::DictionaryValue> val(
          base::DictionaryValue::From(reader.ReadToValueDeprecated(input_js)));
      if (val)
        val->GetString("message", &message);
    }
    file.Close();
  }

  base::DeleteFile(info_path, false);

  return message;
}

leveldb::Status InternalInconsistencyStatus() {
  return leveldb::Status::Corruption("Internal inconsistency");
}

leveldb::Status InvalidDBKeyStatus() {
  return leveldb::Status::InvalidArgument("Invalid database key ID");
}

leveldb::Status IOErrorStatus() {
  return leveldb::Status::IOError("IO Error");
}

Status GetInt(LevelDBTransaction* txn,
              const StringPiece& key,
              int64_t* found_int,
              bool* found) {
  return GetIntInternal(txn, key, found_int, found);
}

Status GetInt(LevelDBDatabase* db,
              const StringPiece& key,
              int64_t* found_int,
              bool* found) {
  return GetIntInternal(db, key, found_int, found);
}

void PutBool(LevelDBTransaction* transaction,
             const StringPiece& key,
             bool value) {
  std::string buffer;
  EncodeBool(value, &buffer);
  transaction->Put(key, &buffer);
}

void PutInt(LevelDBTransaction* transaction,
            const StringPiece& key,
            int64_t value) {
  DCHECK_GE(value, 0);
  std::string buffer;
  EncodeInt(value, &buffer);
  transaction->Put(key, &buffer);
}

template <typename DBOrTransaction>
Status GetVarInt(DBOrTransaction* db,
                 const StringPiece& key,
                 int64_t* found_int,
                 bool* found) {
  std::string result;
  Status s = db->Get(key, &result, found);
  if (!s.ok())
    return s;
  if (!*found)
    return Status::OK();
  StringPiece slice(result);
  if (DecodeVarInt(&slice, found_int) && slice.empty())
    return s;
  return InternalInconsistencyStatus();
}

template Status GetVarInt<LevelDBTransaction>(LevelDBTransaction* txn,
                                              const StringPiece& key,
                                              int64_t* found_int,
                                              bool* found);
template Status GetVarInt<LevelDBDatabase>(LevelDBDatabase* db,
                                           const StringPiece& key,
                                           int64_t* found_int,
                                           bool* found);

void PutVarInt(LevelDBTransaction* transaction,
               const StringPiece& key,
               int64_t value) {
  std::string buffer;
  EncodeVarInt(value, &buffer);
  transaction->Put(key, &buffer);
}

template <typename DBOrTransaction>
Status GetString(DBOrTransaction* db,
                 const StringPiece& key,
                 base::string16* found_string,
                 bool* found) {
  std::string result;
  *found = false;
  Status s = db->Get(key, &result, found);
  if (!s.ok())
    return s;
  if (!*found)
    return Status::OK();
  StringPiece slice(result);
  if (DecodeString(&slice, found_string) && slice.empty())
    return s;
  return InternalInconsistencyStatus();
}

template Status GetString<LevelDBTransaction>(LevelDBTransaction* txn,
                                              const StringPiece& key,
                                              base::string16* found_string,
                                              bool* found);
template Status GetString<LevelDBDatabase>(LevelDBDatabase* db,
                                           const StringPiece& key,
                                           base::string16* found_string,
                                           bool* found);

void PutString(LevelDBTransaction* transaction,
               const StringPiece& key,
               const base::string16& value) {
  std::string buffer;
  EncodeString(value, &buffer);
  transaction->Put(key, &buffer);
}

void PutIDBKeyPath(LevelDBTransaction* transaction,
                   const StringPiece& key,
                   const IndexedDBKeyPath& value) {
  std::string buffer;
  EncodeIDBKeyPath(value, &buffer);
  transaction->Put(key, &buffer);
}

template <typename DBOrTransaction>
Status GetMaxObjectStoreId(DBOrTransaction* db,
                           int64_t database_id,
                           int64_t* max_object_store_id) {
  const std::string max_object_store_id_key = DatabaseMetaDataKey::Encode(
      database_id, DatabaseMetaDataKey::MAX_OBJECT_STORE_ID);
  *max_object_store_id = -1;
  bool found = false;
  Status s = indexed_db::GetInt(db, max_object_store_id_key,
                                max_object_store_id, &found);
  if (!s.ok())
    return s;
  if (!found)
    *max_object_store_id = 0;

  DCHECK_GE(*max_object_store_id, 0);
  return s;
}

template Status GetMaxObjectStoreId<LevelDBTransaction>(
    LevelDBTransaction* db,
    int64_t database_id,
    int64_t* max_object_store_id);
template Status GetMaxObjectStoreId<LevelDBDatabase>(
    LevelDBDatabase* db,
    int64_t database_id,
    int64_t* max_object_store_id);

Status SetMaxObjectStoreId(LevelDBTransaction* transaction,
                           int64_t database_id,
                           int64_t object_store_id) {
  const std::string max_object_store_id_key = DatabaseMetaDataKey::Encode(
      database_id, DatabaseMetaDataKey::MAX_OBJECT_STORE_ID);
  int64_t max_object_store_id = -1;
  bool found = false;
  Status s = GetInt(transaction, max_object_store_id_key, &max_object_store_id,
                    &found);
  if (!s.ok())
    return s;
  if (!found)
    max_object_store_id = 0;

  DCHECK_GE(max_object_store_id, 0);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(SET_MAX_OBJECT_STORE_ID);
    return s;
  }

  if (object_store_id <= max_object_store_id) {
    INTERNAL_CONSISTENCY_ERROR(SET_MAX_OBJECT_STORE_ID);
    return indexed_db::InternalInconsistencyStatus();
  }
  indexed_db::PutInt(transaction, max_object_store_id_key, object_store_id);
  return s;
}

Status GetNewVersionNumber(LevelDBTransaction* transaction,
                           int64_t database_id,
                           int64_t object_store_id,
                           int64_t* new_version_number) {
  const std::string last_version_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::LAST_VERSION);

  *new_version_number = -1;
  int64_t last_version = -1;
  bool found = false;
  Status s = GetInt(transaction, last_version_key, &last_version, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(GET_NEW_VERSION_NUMBER);
    return s;
  }
  if (!found)
    last_version = 0;

  DCHECK_GE(last_version, 0);

  int64_t version = last_version + 1;
  PutInt(transaction, last_version_key, version);

  // TODO(jsbell): Think about how we want to handle the overflow scenario.
  DCHECK(version > last_version);

  *new_version_number = version;
  return s;
}

Status SetMaxIndexId(LevelDBTransaction* transaction,
                     int64_t database_id,
                     int64_t object_store_id,
                     int64_t index_id) {
  int64_t max_index_id = -1;
  const std::string max_index_id_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::MAX_INDEX_ID);
  bool found = false;
  Status s = GetInt(transaction, max_index_id_key, &max_index_id, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(SET_MAX_INDEX_ID);
    return s;
  }
  if (!found)
    max_index_id = kMinimumIndexId;

  if (index_id <= max_index_id) {
    INTERNAL_CONSISTENCY_ERROR_UNTESTED(SET_MAX_INDEX_ID);
    return InternalInconsistencyStatus();
  }

  PutInt(transaction, max_index_id_key, index_id);
  return s;
}

Status VersionExists(LevelDBTransaction* transaction,
                     int64_t database_id,
                     int64_t object_store_id,
                     int64_t version,
                     const std::string& encoded_primary_key,
                     bool* exists) {
  const std::string key =
      ExistsEntryKey::Encode(database_id, object_store_id, encoded_primary_key);
  std::string data;

  Status s = transaction->Get(key, &data, exists);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(VERSION_EXISTS);
    return s;
  }
  if (!*exists)
    return s;

  StringPiece slice(data);
  int64_t decoded;
  if (!DecodeInt(&slice, &decoded) || !slice.empty())
    return InternalInconsistencyStatus();
  *exists = (decoded == version);
  return s;
}

Status GetNewDatabaseId(LevelDBTransaction* transaction, int64_t* new_id) {
  *new_id = -1;
  int64_t max_database_id = -1;
  bool found = false;
  Status s = indexed_db::GetInt(transaction, MaxDatabaseIdKey::Encode(),
                                &max_database_id, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(GET_NEW_DATABASE_ID);
    return s;
  }
  if (!found)
    max_database_id = 0;

  DCHECK_GE(max_database_id, 0);

  int64_t database_id = max_database_id + 1;
  indexed_db::PutInt(transaction, MaxDatabaseIdKey::Encode(), database_id);
  *new_id = database_id;
  return Status::OK();
}

bool CheckObjectStoreAndMetaDataType(const LevelDBIterator* it,
                                     const std::string& stop_key,
                                     int64_t object_store_id,
                                     int64_t meta_data_type) {
  if (!it->IsValid() || CompareKeys(it->Key(), stop_key) >= 0)
    return false;

  StringPiece slice(it->Key());
  ObjectStoreMetaDataKey meta_data_key;
  bool ok =
      ObjectStoreMetaDataKey::Decode(&slice, &meta_data_key) && slice.empty();
  DCHECK(ok);
  if (meta_data_key.ObjectStoreId() != object_store_id)
    return false;
  if (meta_data_key.MetaDataType() != meta_data_type)
    return false;
  return ok;
}

bool CheckIndexAndMetaDataKey(const LevelDBIterator* it,
                              const std::string& stop_key,
                              int64_t index_id,
                              unsigned char meta_data_type) {
  if (!it->IsValid() || CompareKeys(it->Key(), stop_key) >= 0)
    return false;

  StringPiece slice(it->Key());
  IndexMetaDataKey meta_data_key;
  bool ok = IndexMetaDataKey::Decode(&slice, &meta_data_key);
  DCHECK(ok);
  if (meta_data_key.IndexId() != index_id)
    return false;
  if (meta_data_key.meta_data_type() != meta_data_type)
    return false;
  return true;
}

bool FindGreatestKeyLessThanOrEqual(LevelDBTransaction* transaction,
                                    const std::string& target,
                                    std::string* found_key,
                                    Status* s) {
  std::unique_ptr<LevelDBIterator> it = transaction->CreateIterator();
  *s = it->Seek(target);
  if (!s->ok())
    return false;

  if (!it->IsValid()) {
    *s = it->SeekToLast();
    if (!s->ok() || !it->IsValid())
      return false;
  }

  while (CompareIndexKeys(it->Key(), target) > 0) {
    *s = it->Prev();
    if (!s->ok() || !it->IsValid())
      return false;
  }

  do {
    *found_key = it->Key().as_string();

    // There can be several index keys that compare equal. We want the last one.
    *s = it->Next();
  } while (s->ok() && it->IsValid() && !CompareIndexKeys(it->Key(), target));

  return true;
}

bool GetBlobKeyGeneratorCurrentNumber(
    LevelDBTransaction* leveldb_transaction,
    int64_t database_id,
    int64_t* blob_key_generator_current_number) {
  const std::string key_gen_key = DatabaseMetaDataKey::Encode(
      database_id, DatabaseMetaDataKey::BLOB_KEY_GENERATOR_CURRENT_NUMBER);

  // Default to initial number if not found.
  int64_t cur_number = DatabaseMetaDataKey::kBlobKeyGeneratorInitialNumber;
  std::string data;

  bool found = false;
  bool ok = leveldb_transaction->Get(key_gen_key, &data, &found).ok();
  if (!ok) {
    INTERNAL_READ_ERROR_UNTESTED(GET_BLOB_KEY_GENERATOR_CURRENT_NUMBER);
    return false;
  }
  if (found) {
    StringPiece slice(data);
    if (!DecodeVarInt(&slice, &cur_number) || !slice.empty() ||
        !DatabaseMetaDataKey::IsValidBlobKey(cur_number)) {
      INTERNAL_READ_ERROR_UNTESTED(GET_BLOB_KEY_GENERATOR_CURRENT_NUMBER);
      return false;
    }
  }
  *blob_key_generator_current_number = cur_number;
  return true;
}

bool UpdateBlobKeyGeneratorCurrentNumber(
    LevelDBTransaction* leveldb_transaction,
    int64_t database_id,
    int64_t blob_key_generator_current_number) {
#if DCHECK_IS_ON()
  int64_t old_number;
  if (!GetBlobKeyGeneratorCurrentNumber(leveldb_transaction, database_id,
                                        &old_number))
    return false;
  DCHECK_LT(old_number, blob_key_generator_current_number);
#endif
  DCHECK(
      DatabaseMetaDataKey::IsValidBlobKey(blob_key_generator_current_number));
  const std::string key = DatabaseMetaDataKey::Encode(
      database_id, DatabaseMetaDataKey::BLOB_KEY_GENERATOR_CURRENT_NUMBER);

  PutVarInt(leveldb_transaction, key, blob_key_generator_current_number);
  return true;
}

Status GetEarliestSweepTime(LevelDBDatabase* db, base::Time* earliest_sweep) {
  const std::string earliest_sweep_time_key = EarliestSweepKey::Encode();
  *earliest_sweep = base::Time();
  bool found = false;
  int64_t time_micros = 0;
  Status s =
      indexed_db::GetInt(db, earliest_sweep_time_key, &time_micros, &found);
  if (!s.ok())
    return s;
  if (!found)
    time_micros = 0;

  DCHECK_GE(time_micros, 0);
  *earliest_sweep += base::TimeDelta::FromMicroseconds(time_micros);

  return s;
}

void SetEarliestSweepTime(LevelDBTransaction* txn, base::Time earliest_sweep) {
  const std::string earliest_sweep_time_key = EarliestSweepKey::Encode();
  int64_t time_micros = (earliest_sweep - base::Time()).InMicroseconds();
  indexed_db::PutInt(txn, earliest_sweep_time_key, time_micros);
}

const LevelDBComparator* GetDefaultIndexedDBComparator() {
  static const base::NoDestructor<IDBComparator> kIDBComparator;
  return kIDBComparator.get();
}

const leveldb::Comparator* GetDefaultLevelDBComparator() {
  static const base::NoDestructor<LDBComparator> ldb_comparator;
  return ldb_comparator.get();
}

std::tuple<base::FilePath /*leveldb_path*/,
           base::FilePath /*blob_path*/,
           leveldb::Status>
CreateDatabaseDirectories(const base::FilePath& path_base,
                          const url::Origin& origin) {
  leveldb::Status status;
  if (!base::CreateDirectoryAndGetError(path_base, nullptr)) {
    status = Status::IOError("Unable to create IndexedDB database path");
    LOG(ERROR) << status.ToString() << ": \"" << path_base.AsUTF8Unsafe()
               << "\"";
    ReportOpenStatus(indexed_db::INDEXED_DB_BACKING_STORE_OPEN_FAILED_DIRECTORY,
                     origin);
    return {base::FilePath(), base::FilePath(), status};
  }

  base::FilePath leveldb_path = path_base.Append(GetLevelDBFileName(origin));
  base::FilePath blob_path = path_base.Append(GetBlobStoreFileName(origin));
  if (IsPathTooLong(leveldb_path)) {
    ReportOpenStatus(indexed_db::INDEXED_DB_BACKING_STORE_OPEN_ORIGIN_TOO_LONG,
                     origin);
    status = Status::IOError("File path too long");
    return {base::FilePath(), base::FilePath(), status};
  }
  return {leveldb_path, blob_path, status};
}

std::tuple<std::unique_ptr<LevelDBDatabase>,
           leveldb::Status,
           IndexedDBDataLossInfo,
           bool /* is_disk_full */>
OpenAndVerifyLevelDBDatabase(
    const url::Origin& origin,
    base::FilePath path_base,
    base::FilePath database_path,
    LevelDBFactory* ldb_factory,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  // Please see docs/open_and_verify_leveldb_database.code2flow, and the
  // generated pdf (from https://code2flow.com).
  // The intended strategy here is to have this function match that flowchart,
  // where the flowchart should be seen as the 'master' logic template. Please
  // check the git history of both to make sure they are supposed to be in sync.
  DCHECK_EQ(database_path.empty(), path_base.empty());
  IDB_TRACE("indexed_db::OpenAndVerifyLevelDBDatabase");
  bool is_disk_full;
  std::unique_ptr<LevelDBDatabase> database;
  leveldb::Status status;
  scoped_refptr<LevelDBState> state;
  std::tie(state, status, is_disk_full) = ldb_factory->OpenLevelDBState(
      database_path, GetDefaultIndexedDBComparator(),
      GetDefaultLevelDBComparator());
  bool is_schema_known = false;
  // On I/O error the database isn't deleted, in case the issue is temporary.
  if (status.IsIOError()) {
    ReportOpenStatus(indexed_db::INDEXED_DB_BACKING_STORE_OPEN_NO_RECOVERY,
                     origin);
    return {std::move(database), status, IndexedDBDataLossInfo(), is_disk_full};
  }

  IndexedDBDataLossInfo data_loss_info;
  data_loss_info.status = blink::mojom::IDBDataLoss::None;
  if (status.IsCorruption()) {
    // On corruption, recovery will happen in the next section.
    data_loss_info.status = blink::mojom::IDBDataLoss::Total;
    data_loss_info.message = leveldb_env::GetCorruptionMessage(status);
    std::tie(database, status, is_disk_full) = DeleteAndRecreateDatabase(
        origin, database_path, ldb_factory, task_runner);
    // If successful, then the database should be empty and doesn't need any of
    // the corruption or schema checks below.
    if (status.ok()) {
      ReportOpenStatus(indexed_db::INDEXED_DB_BACKING_STORE_OPEN_SUCCESS,
                       origin);
    }
    return {std::move(database), status, data_loss_info, is_disk_full};
  }
  // The leveldb database is successfully opened.
  DCHECK(status.ok());
  database = std::make_unique<LevelDBDatabase>(
      std::move(state), ldb_factory, std::move(task_runner),
      LevelDBDatabase::kDefaultMaxOpenIteratorsPerDatabase);

  // Check for previous corruption or invalid schemas.
  std::string corruption_message;
  if (!path_base.empty())
    corruption_message = ReadCorruptionInfo(path_base, origin);
  if (!corruption_message.empty()) {
    LOG(ERROR) << "IndexedDB recovering from a corrupted (and deleted) "
                  "database.";
    ReportOpenStatus(
        indexed_db::INDEXED_DB_BACKING_STORE_OPEN_FAILED_PRIOR_CORRUPTION,
        origin);
    status = leveldb::Status::Corruption(corruption_message);
    database.reset();
    data_loss_info.status = blink::mojom::IDBDataLoss::Total;
    data_loss_info.message =
        "IndexedDB (database was corrupt): " + corruption_message;
  } else if (!IsSchemaKnown(database.get(), &is_schema_known)) {
    LOG(ERROR) << "IndexedDB had IO error checking schema, treating it as "
                  "failure to open";
    ReportOpenStatus(
        indexed_db::
            INDEXED_DB_BACKING_STORE_OPEN_FAILED_IO_ERROR_CHECKING_SCHEMA,
        origin);
    database.reset();
    data_loss_info.status = blink::mojom::IDBDataLoss::Total;
    data_loss_info.message = "I/O error checking schema";
  } else if (!is_schema_known) {
    LOG(ERROR) << "IndexedDB backing store had unknown schema, treating it "
                  "as failure to open";
    ReportOpenStatus(
        indexed_db::INDEXED_DB_BACKING_STORE_OPEN_FAILED_UNKNOWN_SCHEMA,
        origin);
    database.reset();
    data_loss_info.status = blink::mojom::IDBDataLoss::Total;
    data_loss_info.message = "Unknown schema";
  }
  // Try to delete & recreate the database for any of the above issues.
  if (!database.get()) {
    DCHECK(!is_schema_known || status.IsCorruption());
    std::tie(database, status, is_disk_full) = DeleteAndRecreateDatabase(
        origin, database_path, ldb_factory, task_runner);
  }

  if (status.ok())
    ReportOpenStatus(indexed_db::INDEXED_DB_BACKING_STORE_OPEN_SUCCESS, origin);
  return {std::move(database), status, data_loss_info, is_disk_full};
}

}  // namespace indexed_db
}  // namespace content
