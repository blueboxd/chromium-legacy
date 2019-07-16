// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/hint_cache_store.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"
#include "components/optimization_guide/optimization_guide_prefs.h"
#include "components/optimization_guide/proto/hint_cache.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace optimization_guide {

namespace {

// Enforce that StoreEntryType enum is synced with the StoreEntryType proto
// (components/previews/content/proto/hint_cache.proto)
static_assert(proto::StoreEntryType_MAX ==
                  static_cast<int>(HintCacheStore::StoreEntryType::kMaxValue),
              "mismatched StoreEntryType enums");

// The folder where the data will be stored on disk.
constexpr char kHintCacheStoreFolder[] = "previews_hint_cache_store";

// The amount of data to build up in memory before converting to a sorted on-
// disk file.
constexpr size_t kDatabaseWriteBufferSizeBytes = 128 * 1024;

// Delimiter that appears between the sections of a store entry key.
//  Examples:
//    "[StoreEntryType::kMetadata]_[MetadataType]"
//    "[StoreEntryType::kComponentHint]_[component_version]_[host]"
constexpr char kKeySectionDelimiter = '_';

// Realistic minimum length of a host suffix.
const int kMinHostSuffix = 6;  // eg., abc.tv

constexpr char kHintLoadedPercentageTotalKey[] = "Total";

// Enumerates the possible outcomes of loading metadata. Used in UMA histograms,
// so the order of enumerators should not be changed.
//
// Keep in sync with OptimizationGuideHintCacheLevelDBStoreLoadMetadataResult
// in tools/metrics/histograms/enums.xml.
enum class OptimizationGuideHintCacheLevelDBStoreLoadMetadataResult {
  kSuccess = 0,
  kLoadMetadataFailed = 1,
  kSchemaMetadataMissing = 2,
  kSchemaMetadataWrongVersion = 3,
  kComponentMetadataMissing = 4,
  kFetchedMetadataMissing = 5,
  kComponentAndFetchedMetadataMissing = 6,
  kMaxValue = kComponentAndFetchedMetadataMissing,
};

// Util class for recording the result of loading the metadata. The result is
// recorded when it goes out of scope and its destructor is called.
class ScopedLoadMetadataResultRecorder {
 public:
  ScopedLoadMetadataResultRecorder()
      : result_(OptimizationGuideHintCacheLevelDBStoreLoadMetadataResult::
                    kSuccess) {}
  ~ScopedLoadMetadataResultRecorder() {
    UMA_HISTOGRAM_ENUMERATION(
        "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult", result_);
  }

  void set_result(
      OptimizationGuideHintCacheLevelDBStoreLoadMetadataResult result) {
    result_ = result;
  }

 private:
  OptimizationGuideHintCacheLevelDBStoreLoadMetadataResult result_;
};

void RecordStatusChange(HintCacheStore::Status status) {
  UMA_HISTOGRAM_ENUMERATION("OptimizationGuide.HintCacheLevelDBStore.Status",
                            status);
}

// Returns true if |key_prefix| is a prefix of |key|.
bool DatabasePrefixFilter(const std::string& key_prefix,
                          const std::string& key) {
  return base::StartsWith(key, key_prefix, base::CompareCase::SENSITIVE);
}

std::string GetStringNameForStoreEntryType(
    HintCacheStore::StoreEntryType store_entry_type) {
  switch (store_entry_type) {
    case HintCacheStore::StoreEntryType::kEmpty:
      return "Empty";
    case HintCacheStore::StoreEntryType::kMetadata:
      return "Metadata";
    case HintCacheStore::StoreEntryType::kComponentHint:
      return "ComponentHint";
    case HintCacheStore::StoreEntryType::kFetchedHint:
      return "FetchedHint";
  }
  NOTREACHED();
  return std::string();
}

void IncrementHintLoadedCountsPrefForKey(PrefService* pref_service,
                                         const std::string& key) {
  if (!pref_service)
    return;

  DictionaryPrefUpdate pref(pref_service, prefs::kHintLoadedCounts);
  base::Optional<int> maybe_val = pref->FindIntKey(key);
  if (maybe_val.has_value()) {
    pref->SetIntKey(key, maybe_val.value() + 1);
  } else {
    pref->SetIntKey(key, 1);
  }
}

}  // namespace

HintCacheStore::HintCacheStore(
    leveldb_proto::ProtoDatabaseProvider* database_provider,
    const base::FilePath& database_dir,
    PrefService* pref_service,
    scoped_refptr<base::SequencedTaskRunner> store_task_runner)
    : pref_service_(pref_service) {
  base::FilePath hint_store_dir =
      database_dir.AppendASCII(kHintCacheStoreFolder);
  database_ = database_provider->GetDB<proto::StoreEntry>(
      leveldb_proto::ProtoDbType::HINT_CACHE_STORE, hint_store_dir,
      store_task_runner);

  RecordStatusChange(status_);
}

HintCacheStore::HintCacheStore(
    std::unique_ptr<leveldb_proto::ProtoDatabase<proto::StoreEntry>> database,
    PrefService* pref_service)
    : database_(std::move(database)), pref_service_(pref_service) {
  RecordStatusChange(status_);
}

HintCacheStore::~HintCacheStore() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void HintCacheStore::Initialize(bool purge_existing_data,
                                base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateStatus(Status::kInitializing);

  // Asynchronously initialize the store and run the callback once
  // initialization completes. Initialization consists of the following steps:
  //   1. Initialize the database.
  //   2. If |purge_existing_data| is set to true, unconditionally purge
  //      database and skip to step 6.
  //   3. Otherwise, retrieve the metadata entries (e.g. Schema and Component).
  //   4. If schema is the wrong version, purge database and skip to step 6.
  //   5. Otherwise, load all hint entry keys.
  //   6. Run callback after purging database or retrieving hint entry keys.

  leveldb_env::Options options = leveldb_proto::CreateSimpleOptions();
  options.write_buffer_size = kDatabaseWriteBufferSizeBytes;
  database_->Init(options,
                  base::BindOnce(&HintCacheStore::OnDatabaseInitialized,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 purge_existing_data, std::move(callback)));
}

std::unique_ptr<HintUpdateData>
HintCacheStore::MaybeCreateUpdateDataForComponentHints(
    const base::Version& version) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(version.IsValid());

  if (!IsAvailable()) {
    return nullptr;
  }

  // Component updates are only permitted when the update version is newer than
  // the store's current one.
  if (component_version_.has_value() && version <= component_version_) {
    return nullptr;
  }

  // Create and return a HintUpdateData object. This object has
  // hints from the component moved into it and organizes them in a format
  // usable by the store; the object will returned to the store in
  // StoreComponentHints().
  return HintUpdateData::CreateComponentHintUpdateData(version);
}

std::unique_ptr<HintUpdateData> HintCacheStore::CreateUpdateDataForFetchedHints(
    base::Time update_time,
    base::Time expiry_time) const {
  // Create and returns a HintUpdateData object. This object has has hints
  // from the GetHintsResponse moved into and organizes them in a format
  // usable by the store. The object will be store with UpdateFetchedData().
  return HintUpdateData::CreateFetchedHintUpdateData(update_time, expiry_time);
}

void HintCacheStore::UpdateComponentHints(
    std::unique_ptr<HintUpdateData> component_data,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(component_data);
  DCHECK(!data_update_in_flight_);
  DCHECK(component_data->component_version());

  if (!IsAvailable()) {
    std::move(callback).Run();
    return;
  }

  // If this isn't a newer component version than the store's current one, then
  // the simply return. There's nothing to update.
  if (component_version_.has_value() &&
      component_data->component_version() <= component_version_) {
    std::move(callback).Run();
    return;
  }

  // Mark that there's now a component data update in-flight. While this is
  // true, keys and hints will not be returned by the store.
  data_update_in_flight_ = true;

  // Set the component version prior to requesting the update. This ensures that
  // a second update request for the same component version won't be allowed. In
  // the case where the update fails, the store will become unavailable, so it's
  // safe to treat component version in the update as the new one.
  SetComponentVersion(*component_data->component_version());

  // The current keys are about to be removed, so clear out the keys available
  // within the store. The keys will be populated after the component data
  // update completes.
  hint_entry_keys_.reset();

  // Purge any component hints that are missing the new component version
  // prefix.
  EntryKeyPrefix retain_prefix =
      GetComponentHintEntryKeyPrefix(component_version_.value());
  EntryKeyPrefix filter_prefix = GetComponentHintEntryKeyPrefixWithoutVersion();

  // Received a new component version, report how many unique hints were used
  // and clear the pref afterwards.
  if (pref_service_) {
    const base::DictionaryValue* hint_loaded_counts =
        pref_service_->GetDictionary(prefs::kHintLoadedCounts);
    if (hint_loaded_counts) {
      base::Optional<int> maybe_total_load_attempts =
          hint_loaded_counts->FindIntKey(kHintLoadedPercentageTotalKey);
      if (maybe_total_load_attempts.has_value() &&
          maybe_total_load_attempts.value() > 0) {
        const int total_load_attempts = maybe_total_load_attempts.value();
        int total_hints_loaded = 0;
        for (const auto& it : hint_loaded_counts->DictItems()) {
          // Skip over the total since we already extracted the value.
          if (it.first == kHintLoadedPercentageTotalKey) {
            continue;
          }
          const int val = it.second.GetInt();
          total_hints_loaded += val;
          base::UmaHistogramPercentage(
              base::StrCat(
                  {"OptimizationGuide.HintsLoadedPercentage.", it.first}),
              100 * val / total_load_attempts);
        }

        base::UmaHistogramPercentage(
            "OptimizationGuide.HintsLoadedPercentage",
            100 * total_hints_loaded / total_load_attempts);
      }
    }
    pref_service_->ClearPref(prefs::kHintLoadedCounts);
  }

  // Add the new component data and purge any old component hints from the db.
  // After processing finishes, OnUpdateHints() is called, which loads
  // the updated hint entry keys from the database.
  database_->UpdateEntriesWithRemoveFilter(
      component_data->TakeUpdateEntries(),
      base::BindRepeating(
          [](const EntryKeyPrefix& retain_prefix,
             const EntryKeyPrefix& filter_prefix, const std::string& key) {
            return key.compare(0, retain_prefix.length(), retain_prefix) != 0 &&
                   key.compare(0, filter_prefix.length(), filter_prefix) == 0;
          },
          retain_prefix, filter_prefix),
      base::BindOnce(&HintCacheStore::OnUpdateHints,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void HintCacheStore::UpdateFetchedHints(
    std::unique_ptr<HintUpdateData> fetched_hints_data,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(fetched_hints_data);
  DCHECK(!data_update_in_flight_);
  DCHECK(fetched_hints_data->fetch_update_time());

  if (!IsAvailable()) {
    std::move(callback).Run();
    return;
  }

  fetched_update_time_ = *fetched_hints_data->fetch_update_time();

  data_update_in_flight_ = true;

  hint_entry_keys_.reset();

  // This will remove the fetched metadata entry and insert all the entries
  // currently in |leveldb_fetched_hints_data|.
  database_->UpdateEntriesWithRemoveFilter(
      fetched_hints_data->TakeUpdateEntries(),
      base::BindRepeating(&DatabasePrefixFilter,
                          GetMetadataTypeEntryKey(MetadataType::kFetched)),
      base::BindOnce(&HintCacheStore::OnUpdateHints,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void HintCacheStore::PurgeExpiredFetchedHints() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsAvailable()) {
    return;
  }

  // Load all the fetched hints to check their expiry times.
  database_->LoadKeysAndEntriesWithFilter(
      base::BindRepeating(&DatabasePrefixFilter,
                          GetFetchedHintEntryKeyPrefix()),
      base::BindOnce(&HintCacheStore::OnLoadFetchedHintsToPurgeExpired,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HintCacheStore::OnLoadFetchedHintsToPurgeExpired(
    bool success,
    std::unique_ptr<EntryMap> fetched_entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!success) {
    return;
  }

  auto keys_to_remove = std::make_unique<EntryKeySet>();
  int64_t now_since_epoch =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds();

  for (const auto& entry : *fetched_entries) {
    if (entry.second.expiry_time_secs() <= now_since_epoch) {
      keys_to_remove->insert(entry.first);
    }
  }

  // TODO(mcrouse): Record the number of hints that will be expired from the
  // store.

  data_update_in_flight_ = true;
  hint_entry_keys_.reset();

  auto empty_entries = std::make_unique<EntryVector>();

  database_->UpdateEntriesWithRemoveFilter(
      std::move(empty_entries),
      base::BindRepeating(
          [](EntryKeySet* keys_to_remove, const std::string& key) {
            return keys_to_remove->find(key) != keys_to_remove->end();
          },
          keys_to_remove.get()),
      base::BindOnce(&HintCacheStore::OnUpdateHints,
                     weak_ptr_factory_.GetWeakPtr(), base::DoNothing::Once()));
}

bool HintCacheStore::FindHintEntryKey(const std::string& host,
                                      EntryKey* out_hint_entry_key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Search for kFetched hint entry keys first, fetched hints should be
  // fresher and preferred.
  if (FindHintEntryKeyForHostWithPrefix(host, out_hint_entry_key,
                                        GetFetchedHintEntryKeyPrefix())) {
    return true;
  }

  // Search for kComponent hint entry keys next.
  DCHECK(!component_version_.has_value() ||
         component_hint_entry_key_prefix_ ==
             GetComponentHintEntryKeyPrefix(component_version_.value()));
  if (FindHintEntryKeyForHostWithPrefix(host, out_hint_entry_key,
                                        component_hint_entry_key_prefix_))
    return true;

  return false;
}

bool HintCacheStore::FindHintEntryKeyForHostWithPrefix(
    const std::string& host,
    EntryKey* out_hint_entry_key,
    const EntryKeyPrefix& hint_entry_key_prefix) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(out_hint_entry_key);

  // Look for longest host name suffix that has a hint. No need to continue
  // lookups and substring work once get to a root domain like ".com" or
  // ".co.in" (MinHostSuffix length check is a heuristic for that).
  std::string host_suffix(host);
  while (host_suffix.length() >= kMinHostSuffix) {
    // Attempt to find a hint entry key associated with the current host suffix.
    *out_hint_entry_key = hint_entry_key_prefix + host_suffix;
    if (hint_entry_keys_ && hint_entry_keys_->find(*out_hint_entry_key) !=
                                hint_entry_keys_->end()) {
      return true;
    }

    size_t pos = host_suffix.find_first_of('.');
    if (pos == std::string::npos) {
      break;
    }
    host_suffix = host_suffix.substr(pos + 1);
  }
  return false;
}

void HintCacheStore::LoadHint(const EntryKey& hint_entry_key,
                              HintLoadedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsAvailable()) {
    std::move(callback).Run(hint_entry_key, nullptr);
    return;
  }

  IncrementHintLoadedCountsPrefForKey(pref_service_,
                                      kHintLoadedPercentageTotalKey);

  database_->GetEntry(hint_entry_key,
                      base::BindOnce(&HintCacheStore::OnLoadHint,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     hint_entry_key, std::move(callback)));
}

base::Time HintCacheStore::FetchedHintsUpdateTime() const {
  // If the store is not available, the metadata entries have not been loaded
  // so there are no fetched hints.
  if (!IsAvailable())
    return base::Time();
  return fetched_update_time_;
}

// static
const char HintCacheStore::kStoreSchemaVersion[] = "1";

// static
HintCacheStore::EntryKeyPrefix HintCacheStore::GetMetadataEntryKeyPrefix() {
  return base::NumberToString(
             static_cast<int>(HintCacheStore::StoreEntryType::kMetadata)) +
         kKeySectionDelimiter;
}

// static
HintCacheStore::EntryKey HintCacheStore::GetMetadataTypeEntryKey(
    MetadataType metadata_type) {
  return GetMetadataEntryKeyPrefix() +
         base::NumberToString(static_cast<int>(metadata_type));
}

// static
HintCacheStore::EntryKeyPrefix
HintCacheStore::GetComponentHintEntryKeyPrefixWithoutVersion() {
  return base::NumberToString(
             static_cast<int>(HintCacheStore::StoreEntryType::kComponentHint)) +
         kKeySectionDelimiter;
}

// static
HintCacheStore::EntryKeyPrefix HintCacheStore::GetComponentHintEntryKeyPrefix(
    const base::Version& component_version) {
  return GetComponentHintEntryKeyPrefixWithoutVersion() +
         component_version.GetString() + kKeySectionDelimiter;
}

// static
HintCacheStore::EntryKeyPrefix HintCacheStore::GetFetchedHintEntryKeyPrefix() {
  return base::NumberToString(
             static_cast<int>(HintCacheStore::StoreEntryType::kFetchedHint)) +
         kKeySectionDelimiter;
}

void HintCacheStore::UpdateStatus(Status new_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Status::kUninitialized can only transition to Status::kInitializing.
  DCHECK(status_ != Status::kUninitialized ||
         new_status == Status::kInitializing);
  // Status::kInitializing can only transition to Status::kAvailable or
  // Status::kFailed.
  DCHECK(status_ != Status::kInitializing || new_status == Status::kAvailable ||
         new_status == Status::kFailed);
  // Status::kAvailable can only transition to kStatus::Failed.
  DCHECK(status_ != Status::kAvailable || new_status == Status::kFailed);
  // The status can never transition from Status::kFailed.
  DCHECK(status_ != Status::kFailed || new_status == Status::kFailed);

  // If the status is not changing, simply return; the remaining logic handles
  // status changes.
  if (status_ == new_status) {
    return;
  }

  status_ = new_status;
  RecordStatusChange(status_);

  if (status_ == Status::kFailed) {
    database_->Destroy(base::BindOnce(&HintCacheStore::OnDatabaseDestroyed,
                                      weak_ptr_factory_.GetWeakPtr()));
    ClearComponentVersion();
    hint_entry_keys_.reset();
  }
}

bool HintCacheStore::IsAvailable() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return status_ == Status::kAvailable;
}

void HintCacheStore::PurgeDatabase(base::OnceClosure callback) {
  // When purging the database, update the schema version to the current one.
  EntryKey schema_entry_key = GetMetadataTypeEntryKey(MetadataType::kSchema);
  proto::StoreEntry schema_entry;
  schema_entry.set_version(kStoreSchemaVersion);

  auto entries_to_save = std::make_unique<EntryVector>();
  entries_to_save->emplace_back(schema_entry_key, schema_entry);

  database_->UpdateEntriesWithRemoveFilter(
      std::move(entries_to_save),
      base::BindRepeating(
          [](const std::string& schema_entry_key, const std::string& key) {
            return key.compare(0, schema_entry_key.length(),
                               schema_entry_key) != 0;
          },
          schema_entry_key),
      base::BindOnce(&HintCacheStore::OnPurgeDatabase,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void HintCacheStore::SetComponentVersion(
    const base::Version& component_version) {
  DCHECK(component_version.IsValid());
  component_version_ = component_version;
  component_hint_entry_key_prefix_ =
      GetComponentHintEntryKeyPrefix(component_version_.value());
}

void HintCacheStore::ClearComponentVersion() {
  component_version_.reset();
  component_hint_entry_key_prefix_.clear();
}

void HintCacheStore::ClearFetchedHintsFromDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!data_update_in_flight_);

  if (!IsAvailable()) {
    return;
  }

  data_update_in_flight_ = true;
  auto entries_to_save = std::make_unique<EntryVector>();

  // TODO(mcrouse): Add histogram to record the number of hints being removed.
  hint_entry_keys_.reset();

  // Removes all |kFetchedHint| store entries. OnUpdateHints will handle
  // updating status and re-filling hint_entry_keys with the hints still in the
  // store.
  database_->UpdateEntriesWithRemoveFilter(
      std::move(entries_to_save),  // this should be empty.
      base::BindRepeating(&DatabasePrefixFilter,
                          GetFetchedHintEntryKeyPrefix()),
      base::BindOnce(&HintCacheStore::OnUpdateHints,
                     weak_ptr_factory_.GetWeakPtr(), base::DoNothing::Once()));
}

void HintCacheStore::MaybeLoadHintEntryKeys(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If the database is unavailable or if there's an in-flight component data
  // update, then don't load the hint keys. Simply run the callback.
  if (!IsAvailable() || data_update_in_flight_) {
    std::move(callback).Run();
    return;
  }

  // Create a new KeySet object. This is populated by the store's keys as the
  // filter is run with each key on the DB's background thread. The filter
  // itself always returns false, ensuring that no entries are ever actually
  // loaded by the DB. Ownership of the KeySet is passed into the
  // LoadKeysAndEntriesCallback callback, guaranteeing that the KeySet has a
  // lifespan longer than the filter calls.
  std::unique_ptr<EntryKeySet> hint_entry_keys(std::make_unique<EntryKeySet>());
  EntryKeySet* raw_hint_entry_keys_pointer = hint_entry_keys.get();
  database_->LoadKeysAndEntriesWithFilter(
      base::BindRepeating(
          [](EntryKeySet* hint_entry_keys, const std::string& filter_prefix,
             const std::string& entry_key) {
            if (entry_key.compare(0, filter_prefix.length(), filter_prefix) !=
                0) {
              hint_entry_keys->insert(entry_key);
            }
            return false;
          },
          raw_hint_entry_keys_pointer, GetMetadataEntryKeyPrefix()),
      base::BindOnce(&HintCacheStore::OnLoadHintEntryKeys,
                     weak_ptr_factory_.GetWeakPtr(), std::move(hint_entry_keys),
                     std::move(callback)));
}

size_t HintCacheStore::GetHintEntryKeyCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return hint_entry_keys_ ? hint_entry_keys_->size() : 0;
}

void HintCacheStore::OnDatabaseInitialized(
    bool purge_existing_data,
    base::OnceClosure callback,
    leveldb_proto::Enums::InitStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != leveldb_proto::Enums::InitStatus::kOK) {
    UpdateStatus(Status::kFailed);
    std::move(callback).Run();
    return;
  }

  // If initialization is set to purge all existing data, then simply trigger
  // the purge and return. There's no need to load metadata entries that'll
  // immediately be purged.
  if (purge_existing_data) {
    PurgeDatabase(std::move(callback));
    return;
  }

  // Load all entries from the DB with the metadata key prefix.
  database_->LoadKeysAndEntriesWithFilter(
      leveldb_proto::KeyFilter(), leveldb::ReadOptions(),
      GetMetadataEntryKeyPrefix(),
      base::BindOnce(&HintCacheStore::OnLoadMetadata,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void HintCacheStore::OnDatabaseDestroyed(bool /*success*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void HintCacheStore::OnLoadMetadata(
    base::OnceClosure callback,
    bool success,
    std::unique_ptr<EntryMap> metadata_entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(metadata_entries);

  // Create a scoped load metadata result recorder. It records the result when
  // its destructor is called.
  ScopedLoadMetadataResultRecorder result_recorder;

  if (!success) {
    result_recorder.set_result(
        OptimizationGuideHintCacheLevelDBStoreLoadMetadataResult::
            kLoadMetadataFailed);

    UpdateStatus(Status::kFailed);
    std::move(callback).Run();
    return;
  }

  // If the schema version in the DB is not the current version, then purge
  // the database.
  auto schema_entry =
      metadata_entries->find(GetMetadataTypeEntryKey(MetadataType::kSchema));
  if (schema_entry == metadata_entries->end() ||
      !schema_entry->second.has_version() ||
      schema_entry->second.version() != kStoreSchemaVersion) {
    if (schema_entry == metadata_entries->end()) {
      result_recorder.set_result(
          OptimizationGuideHintCacheLevelDBStoreLoadMetadataResult::
              kSchemaMetadataMissing);
    } else {
      result_recorder.set_result(
          OptimizationGuideHintCacheLevelDBStoreLoadMetadataResult::
              kSchemaMetadataWrongVersion);
    }

    PurgeDatabase(std::move(callback));
    return;
  }

  // If the component metadata entry exists, then use it to set the component
  // version.
  bool component_metadata_missing = false;
  auto component_entry =
      metadata_entries->find(GetMetadataTypeEntryKey(MetadataType::kComponent));
  if (component_entry != metadata_entries->end()) {
    DCHECK(component_entry->second.has_version());
    SetComponentVersion(base::Version(component_entry->second.version()));
  } else {
    result_recorder.set_result(
        OptimizationGuideHintCacheLevelDBStoreLoadMetadataResult::
            kComponentMetadataMissing);
    component_metadata_missing = true;
  }

  auto fetched_entry =
      metadata_entries->find(GetMetadataTypeEntryKey(MetadataType::kFetched));
  if (fetched_entry != metadata_entries->end()) {
    DCHECK(fetched_entry->second.has_update_time_secs());
    fetched_update_time_ = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromSeconds(fetched_entry->second.update_time_secs()));
  } else {
    if (component_metadata_missing) {
      result_recorder.set_result(
          OptimizationGuideHintCacheLevelDBStoreLoadMetadataResult::
              kComponentAndFetchedMetadataMissing);
    } else {
      result_recorder.set_result(
          OptimizationGuideHintCacheLevelDBStoreLoadMetadataResult::
              kFetchedMetadataMissing);
    }
    fetched_update_time_ = base::Time();
  }

  UpdateStatus(Status::kAvailable);
  MaybeLoadHintEntryKeys(std::move(callback));
}

void HintCacheStore::OnPurgeDatabase(base::OnceClosure callback, bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The database can only be purged during initialization.
  DCHECK_EQ(status_, Status::kInitializing);

  UpdateStatus(success ? Status::kAvailable : Status::kFailed);
  std::move(callback).Run();
}

void HintCacheStore::OnUpdateHints(base::OnceClosure callback, bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(data_update_in_flight_);

  data_update_in_flight_ = false;
  if (!success) {
    UpdateStatus(Status::kFailed);
    std::move(callback).Run();
    return;
  }
  MaybeLoadHintEntryKeys(std::move(callback));
}

void HintCacheStore::OnLoadHintEntryKeys(
    std::unique_ptr<EntryKeySet> hint_entry_keys,
    base::OnceClosure callback,
    bool success,
    std::unique_ptr<EntryMap> /*unused*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!hint_entry_keys_);

  if (!success) {
    UpdateStatus(Status::kFailed);
    std::move(callback).Run();
    return;
  }

  // If the store was set to unavailable after the request was started, or if
  // there's an in-flight component data update, which means the keys are
  // about to be invalidated, then the loaded keys should not be considered
  // valid. Reset the keys so that they are cleared.
  if (!IsAvailable() || data_update_in_flight_) {
    hint_entry_keys.reset();
  }

  hint_entry_keys_ = std::move(hint_entry_keys);
  std::move(callback).Run();
}

void HintCacheStore::OnLoadHint(const std::string& entry_key,
                                HintLoadedCallback callback,
                                bool success,
                                std::unique_ptr<proto::StoreEntry> entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If either the request failed, the store was set to unavailable after the
  // request was started, or there's an in-flight component data update, which
  // means the entry is about to be invalidated, then the loaded hint should
  // not be considered valid. Reset the entry so that no hint is returned to
  // the requester.
  if (!success || !IsAvailable() || data_update_in_flight_) {
    entry.reset();
  }

  if (!entry || !entry->has_hint()) {
    std::unique_ptr<proto::Hint> loaded_hint(nullptr);
    std::move(callback).Run(entry_key, std::move(loaded_hint));
    return;
  }

  if (entry->has_expiry_time_secs() &&
      entry->expiry_time_secs() <=
          base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds()) {
    // An expired hint should be loaded rarely if the user is regularly fetching
    // and storing fresh hints. Expired fetched hints are removed each time
    // fresh hints are fetched and placed into the store. In the future, the
    // expired hints could be asynchronously removed if necessary.
    // An empty hint is returned instead of the expired one.
    UMA_HISTOGRAM_BOOLEAN(
        "OptimizationGuide.HintCacheStore.OnLoadHint.FetchedHintExpired", true);
    std::unique_ptr<proto::Hint> loaded_hint(nullptr);
    std::move(callback).Run(entry_key, std::move(loaded_hint));
    return;
  }

  // Release the Hint into a Hint unique_ptr. This eliminates the need for any
  // copies of the entry's hint.
  std::unique_ptr<proto::Hint> loaded_hint(entry->release_hint());

  StoreEntryType store_entry_type =
      static_cast<StoreEntryType>(entry->entry_type());
  UMA_HISTOGRAM_ENUMERATION("OptimizationGuide.HintCache.HintType.Loaded",
                            store_entry_type);

  IncrementHintLoadedCountsPrefForKey(
      pref_service_, GetStringNameForStoreEntryType(store_entry_type));

  if (store_entry_type == StoreEntryType::kFetchedHint) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "OptimizationGuide.HintCache.FetchedHint.TimeToExpiration",
        base::Time::FromDeltaSinceWindowsEpoch(
            base::TimeDelta::FromSeconds(entry->expiry_time_secs())) -
            base::Time::Now(),
        base::TimeDelta::FromHours(1), base::TimeDelta::FromDays(15), 50);
  }
  std::move(callback).Run(entry_key, std::move(loaded_hint));
}

}  // namespace optimization_guide
